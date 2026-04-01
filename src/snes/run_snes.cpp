#include "run_snes.h"
#include <Arduino.h>
#include "share/utils.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "snes_display.h"
#include "snes_stubs.h"
#include "snes_input.h"
#include "snes_video_mode.h"
#include "snes_save.h"

extern "C" {
    #include "snes9x/snes9x.h"
}

/* -------------------------------------------------------------------------- */
/* Globals                                                                    */
/* -------------------------------------------------------------------------- */

static uint32_t g_dstY  = 0;
static uint32_t g_dstY1 = 0;
static uint32_t g_dstY2 = 0;
static bool     g_hasSecond = false;

/* screen mode */
volatile SnesScreenMode g_snesScreenMode = SNES_SCREEN_INTERLACE;

/* runtime adaptive states */
static bool s_interlace_enabled = false;
static bool s_line_dup_enabled  = false;
static uint32_t s_fieldParity   = 0;

/* -------------------------------------------------------------------------- */
/* Callbacks video                                                            */
/* -------------------------------------------------------------------------- */

static void S9XLineRender(uint32_t y,
                          const uint16_t* pixels,
                          uint32_t width)
{
    (void)y;
    snes_display_submit_line(g_dstY, pixels, width);
}

static void S9XLineRenderEx(uint32_t y,
                            const uint16_t* pixels,
                            uint32_t width)
{
    (void)y;
    snes_display_submit_line_ex(g_dstY1, g_dstY2, g_hasSecond, pixels, width);
}

/* -------------------------------------------------------------------------- */
/* Input hook                                                                 */
/* -------------------------------------------------------------------------- */

uint32_t S9xReadJoypad(int32_t port)
{
    if (port != 0) {
        return 0;
    }

    return snes_input_poll();
}

/* -------------------------------------------------------------------------- */
/* Display init hook                                                          */
/* -------------------------------------------------------------------------- */

bool S9xInitDisplay(void)
{
    GFX.Pitch      = SNES_WIDTH * sizeof(uint16_t);
    GFX.Pitch2     = GFX.Pitch;
    GFX.RealPitch  = GFX.Pitch;
    GFX.ZPitch     = SNES_WIDTH;

    GFX.PPL        = SNES_WIDTH;
    GFX.PPLx2      = SNES_WIDTH * 2;

    // pas de framebuffer
    GFX.Screen     = NULL;
    GFX.SubScreen  = NULL;
    GFX.ZBuffer    = NULL;
    GFX.SubZBuffer = NULL;
    GFX.LineRenderMode = true;
    GFX.LinePPL        = SNES_WIDTH;
    GFX.LinePitch      = SNES_WIDTH * sizeof(uint16_t);

    return true;
}

/* -------------------------------------------------------------------------- */
/* Core init                                                                  */
/* -------------------------------------------------------------------------- */

bool snes_init()
{
    if (!S9xInitDisplay()) {
        printf("[SNES] S9xInitDisplay failed\n");
        return false;
    }

    if (!S9xInitMemory()) {
        printf("[SNES] S9xInitMemory failed\n");
        return false;
    }

    if (!S9xInitGFX()) {
        printf("[SNES] S9xInitGFX failed\n");
        return false;
    }

    if (!S9xInitMap()) {
        printf("[SNES] S9xInitMap failed\n");
        return false;
    }

    if (!S9xInitPpu()) {
        printf("[SNES] S9xInitPpu failed\n");
        return false;
    }

    if (!S9xInitLineBuffers()) {
        printf("[SNES] S9xInitLineBuffers failed\n");
        return false;
    }

    if (!LoadROM(NULL)) {
        printf("[SNES] LoadROM failed\n");
        return false;
    }

    S9xFixColourBrightness();
    return true;
}

static inline int32_t clamp_src_y(int32_t y)
{
    if (y < 0) {
        return 0;
    }
    if (y >= (int32_t)PPU.ScreenHeight) {
        return (int32_t)PPU.ScreenHeight - 1;
    }
    return y;
}

/* -------------------------------------------------------------------------- */
/* Render pipelines                                                           */
/* -------------------------------------------------------------------------- */

static void render_interlace_pipeline(bool interlace_enabled)
{
    snes_display_update_transform(SNES_WIDTH, PPU.ScreenHeight);

    SnesDisplayTransform tr;
    snes_display_get_transform(&tr);

    const float srcCY = ((float)PPU.ScreenHeight - 1.0f) * 0.5f;
    const float dstCY = ((float)tr.dstH - 1.0f) * 0.5f;

    const uint32_t startY = interlace_enabled ? s_fieldParity : 0;
    const uint32_t stepY  = interlace_enabled ? 2 : 1;

    for (uint32_t localY = startY; localY < (uint32_t)tr.dstH; localY += stepY) {
        float srcYf = srcCY + ((float)localY - dstCY) * tr.invScaleY;
        int32_t srcY = clamp_src_y((int32_t)srcYf);

        g_dstY = (uint32_t)(tr.yOffset + localY);
        S9xRenderLine_NoFramebuffer((uint32_t)srcY, S9XLineRender);
    }

    if (interlace_enabled) {
        s_fieldParity ^= 1;
    }
}

static void render_line_dup_pipeline(bool line_dup_enabled)
{
    snes_display_update_transform(SNES_WIDTH, PPU.ScreenHeight);

    SnesDisplayTransform tr;
    snes_display_get_transform(&tr);

    const float srcCY = ((float)PPU.ScreenHeight - 1.0f) * 0.5f;
    const float dstCY = ((float)tr.dstH - 1.0f) * 0.5f;

    if (line_dup_enabled) {
        for (int pairY = 0; pairY < tr.dstH; pairY += 2) {
            float srcYf = srcCY + ((float)pairY - dstCY) * tr.invScaleY;
            int32_t srcY = clamp_src_y((int32_t)srcYf);

            g_dstY1 = (uint32_t)(tr.yOffset + pairY);
            g_dstY2 = (pairY + 1 < tr.dstH)
                ? (uint32_t)(tr.yOffset + pairY + 1)
                : g_dstY1;
            g_hasSecond = (g_dstY2 != g_dstY1);

            S9xRenderLine_NoFramebuffer((uint32_t)srcY, S9XLineRenderEx);
        }

        snes_display_wait_idle();
    } else {
        for (int dstY = 0; dstY < tr.dstH; ++dstY) {
            float srcYf = srcCY + ((float)dstY - dstCY) * tr.invScaleY;
            int32_t srcY = clamp_src_y((int32_t)srcYf);

            g_dstY1 = (uint32_t)(tr.yOffset + dstY);
            g_dstY2 = 0;
            g_hasSecond = false;

            S9xRenderLine_NoFramebuffer((uint32_t)srcY, S9XLineRenderEx);
        }
    }
}

static const char* get_screen_mode_string()
{
    switch (g_snesScreenMode) {
        case SNES_SCREEN_INTERLACE:
            return s_interlace_enabled ? "INTERLACE(ON)" : "INTERLACE(OFF)";
        case SNES_SCREEN_LINE_DUPLICATE:
            return s_line_dup_enabled ? "LINE_DUP(ON)" : "LINE_DUP(OFF)";
        default:
            return "UNKNOWN";
    }
}

/* -------------------------------------------------------------------------- */
/* Run                                                                        */
/* -------------------------------------------------------------------------- */

void run_snes(const uint8_t* rom, size_t romSize, const char* romName)
{
    printf("[SNES] ROM: %p (size %zu bytes)\n", rom, romSize);

    Memory.ROM           = (uint8_t*)rom;
    Memory.ROM_Offset    = 0;
    Memory.ROM_AllocSize = romSize;

    Settings.CyclesPercentage = 100;
    Settings.H_Max            = SNES_CYCLES_PER_SCANLINE;
    Settings.FrameTimePAL     = 20000;
    Settings.FrameTimeNTSC    = 16667;
    Settings.ControllerOption = SNES_JOYPAD;
    Settings.HBlankStart      = (256 * Settings.H_Max) / SNES_HCOUNTER_MAX;

    Settings.SoundPlaybackRate = 0;
    Settings.SoundBufferSize   = 0;
    Settings.ThreadSound       = false;
    Settings.Mute              = true;
    Settings.APUEnabled        = false;
    Settings.DisableSoundEcho  = false;

    if (!snes_init()) {
        printf("[SNES] snes_init failed, aborting\n");
        return;
    }

    // Save, up to 2KB of SRAM
    // Disable for now, not enough RAM to write to file 
    // Still allocate SRAM to load games that require it
    snes_save_prepare_sram();
    // snes_save_init(romName);
    // snes_save_load();

    S9xReset();

    s_interlace_enabled = false;
    s_line_dup_enabled  = false;
    s_fieldParity       = 0;

    const int targetFps = 60;
    const uint32_t frame_us = 1000000u / (uint32_t)targetFps;
    uint64_t next_frame_us = esp_timer_get_time();
    uint32_t frameCount = 0;
    uint32_t lastFpsMs  = millis();
    int64_t now;
    int64_t lateness;
    const uint32_t budget55_us = 1000000u / 55u;
    bool skipped_last_render = false;
    uint32_t last_frame_exec_us = 0;

    snes_display_init();
    snes_display_start();
    snes_input_start();

    printf("[SNES] Core/Video only, no audio, no tilecache, %d FPS target\n",
           targetFps);

    heap_caps_check_integrity_all(true);

    while (true) {
        /* frame skip global */
        bool want_skip = (last_frame_exec_us > budget55_us);
        bool do_render = true;

        if (want_skip && !skipped_last_render) {
            do_render = false;
        }

        IPPU.RenderThisFrame = do_render;

        int64_t frame_start_us = esp_timer_get_time();
        S9xMainLoop();
        // Check save after each frame
        // snes_save_tick();

        if (IPPU.RenderThisFrame) {
            if (g_snesScreenMode == SNES_SCREEN_INTERLACE) {
                render_interlace_pipeline(s_interlace_enabled);
            } else {
                render_line_dup_pipeline(s_line_dup_enabled);
            }
        }

        int64_t frame_end_us = esp_timer_get_time();
        last_frame_exec_us = (uint32_t)(frame_end_us - frame_start_us);

        skipped_last_render = !IPPU.RenderThisFrame;

        frameCount++;
        uint32_t nowMs = millis();
        if (nowMs - lastFpsMs >= 1000) {
            float fps = (frameCount * 1000.0f) / (nowMs - lastFpsMs);

            if (g_snesScreenMode == SNES_SCREEN_INTERLACE) {
                /*version interlace */
                if (!s_interlace_enabled && fps < 48.0f) {
                    s_interlace_enabled = true;
                } else if (s_interlace_enabled && fps > 60.0f) {
                    s_interlace_enabled = false;
                }

                s_line_dup_enabled = false;
            } else {
                /* version line dup (lower resolution) */
                if (!s_line_dup_enabled && fps < 48.0f) {
                    s_line_dup_enabled = true;
                } else if (s_line_dup_enabled && fps > 60.0f) {
                    s_line_dup_enabled = false;
                }

                s_interlace_enabled = false;
                s_fieldParity = 0;
            }

            printf("[SNES] FPS: %.2f | HEAP: %u | MODE: %s | ZOOM: %d%% | SKIP: %s\n",
                   fps,
                   esp_get_free_heap_size(),
                   get_screen_mode_string(),
                   snesZoomPercent,
                   skipped_last_render ? "YES" : "NO");

            frameCount = 0;
            lastFpsMs  = nowMs;
        }

        next_frame_us += frame_us;
        now = (int64_t)esp_timer_get_time();
        lateness = now - (int64_t)next_frame_us;

        if (lateness > 0) {
            if (lateness > (int64_t)frame_us) {
                next_frame_us = (uint64_t)now;
            }
            continue;
        } else {
            share::sleep_until_us(next_frame_us);
        }
    }
}