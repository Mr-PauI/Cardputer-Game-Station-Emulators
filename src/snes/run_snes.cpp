#include "run_snes.h"
#include <Arduino.h>
#include "share/utils.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "snes_display.h"
#include "snes_stubs.h"
#include "snes_input.h"

extern "C" {
    #include "snes9x/snes9x.h"
}

/* Globals */
static uint32_t g_dstY1 = 0;
static uint32_t g_dstY2 = 0;
static bool     g_hasSecond = false;

bool interlace_enabled = false;

/* Callback video line render Snes9x */
static void S9XLineRender(uint32_t y,
                          const uint16_t* pixels,
                          uint32_t width)
{
    (void)y;
    snes_display_submit_line_ex(g_dstY1, g_dstY2, g_hasSecond, pixels, width);
}

/* Hook controls Snes9x */
uint32_t S9xReadJoypad(int32_t port)
{
    if (port != 0) {
        return 0;
    }

    return snes_input_poll();
}

/* Hook Init Snes9x render */
bool S9xInitDisplay(void)
{
    GFX.Pitch      = SNES_WIDTH * sizeof(uint16_t);
    GFX.Pitch2     = GFX.Pitch;
    GFX.RealPitch  = GFX.Pitch;
    GFX.ZPitch     = SNES_WIDTH;

    GFX.PPL        = SNES_WIDTH;
    GFX.PPLx2      = SNES_WIDTH * 2;

    // we use line rendering/instant push to the screen, no framebuffer needed
    GFX.Screen     = NULL;
    GFX.SubScreen  = NULL;
    GFX.ZBuffer    = NULL;
    GFX.SubZBuffer = NULL;
    GFX.LineRenderMode = true;
    GFX.LinePPL        = SNES_WIDTH;
    GFX.LinePitch      = SNES_WIDTH * sizeof(uint16_t);

    return true;
}

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

    // NULL means use already mapped ROM
    if (!LoadROM(NULL)) {
        printf("[SNES] LoadROM failed\n");
        return false;
    }

    S9xFixColourBrightness();
    return true;
}

// ----------------------------------------------------
// RUN SNES
// ----------------------------------------------------
void run_snes(const uint8_t* rom, size_t romSize)
{
    printf("[SNES] ROM: %p (size %zu bytes)\n", rom, romSize);

    // Rom (mapped in flash)
    Memory.ROM           = (uint8_t*)rom;
    Memory.ROM_Offset    = 0;
    Memory.ROM_AllocSize = romSize;

    // Timing
    Settings.CyclesPercentage = 100;
    Settings.H_Max            = SNES_CYCLES_PER_SCANLINE;
    Settings.FrameTimePAL     = 20000;
    Settings.FrameTimeNTSC    = 16667;
    Settings.ControllerOption = SNES_JOYPAD;
    Settings.HBlankStart      = (256 * Settings.H_Max) / SNES_HCOUNTER_MAX;

    // Audio OFF (not enough RAM)
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

    // LoadROM could set SRAM size, ensure no SRAM used
    Memory.SRAMSize = 0;
    Memory.SRAMMask = 0;
    Memory.SRAM     = NULL;

    S9xReset();

    // Timing variables
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
    int32_t srcY = 0;

    // Init display and input
    snes_display_init();
    snes_display_start();
    snes_input_start();

    printf("[SNES] Core/Video only, no audio, no sram, no tilecache, %d FPS target\n",
           targetFps);

    heap_caps_check_integrity_all(true);

    while (true) {
        // Decide to skip frame render or not
        bool want_skip = (last_frame_exec_us > budget55_us);
        bool do_render = true;

        if (want_skip && !skipped_last_render) {
            do_render = false;
        }

        IPPU.RenderThisFrame = do_render;

        // Run one frame
        int64_t frame_start_us = esp_timer_get_time();
        S9xMainLoop();

        // Render frame
        if (IPPU.RenderThisFrame) {
            snes_display_update_transform(SNES_WIDTH, PPU.ScreenHeight);

            SnesDisplayTransform tr;
            snes_display_get_transform(&tr);

            const float srcCY = ((float)PPU.ScreenHeight - 1.0f) * 0.5f;
            const float dstCY = ((float)tr.dstH - 1.0f) * 0.5f;

            if (interlace_enabled) {
                // rendering two lines in one go, to gain some FPS, at the cost of vertical resolution 
                for (int pairY = 0; pairY < tr.dstH; pairY += 2) {
                    float srcYf = srcCY + ((float)pairY - dstCY) * tr.invScaleY;
                    srcY = (int32_t)srcYf;

                    if (srcY < 0) {
                        srcY = 0;
                    }
                    if (srcY >= (int32_t)PPU.ScreenHeight) {
                        srcY = (int32_t)PPU.ScreenHeight - 1;
                    }

                    g_dstY1 = (uint32_t)(tr.yOffset + pairY);
                    g_dstY2 = (pairY + 1 < tr.dstH)
                        ? (uint32_t)(tr.yOffset + pairY + 1)
                        : g_dstY1;
                    g_hasSecond = (g_dstY2 != g_dstY1);

                    S9xRenderLine_NoFramebuffer((uint32_t)srcY, S9XLineRender);
                }

                // wait for the small display buffers to be emptied
                // before moving to the next frame
                snes_display_wait_idle();
            } else {
                for (int dstY = 0; dstY < tr.dstH; ++dstY) {
                    float srcYf = srcCY + ((float)dstY - dstCY) * tr.invScaleY;
                    srcY = (int32_t)srcYf;

                    if (srcY < 0) {
                        srcY = 0;
                    }
                    if (srcY >= (int32_t)PPU.ScreenHeight) {
                        srcY = (int32_t)PPU.ScreenHeight - 1;
                    }

                    g_dstY1 = (uint32_t)(tr.yOffset + dstY);
                    g_dstY2 = 0;
                    g_hasSecond = false;

                    S9xRenderLine_NoFramebuffer((uint32_t)srcY, S9XLineRender);
                }
            }
        }

        // Frame end
        int64_t frame_end_us = esp_timer_get_time();
        last_frame_exec_us = (uint32_t)(frame_end_us - frame_start_us);

        skipped_last_render = !IPPU.RenderThisFrame;

        // Log FPS + heap
        frameCount++;
        uint32_t nowMs = millis();
        if (nowMs - lastFpsMs >= 1000) {
            float fps = (frameCount * 1000.0f) / (nowMs - lastFpsMs);

            // Auto interlace toggle
            if (!interlace_enabled && fps < 48.0f) {
                interlace_enabled = true;
            } else if (interlace_enabled && fps > 60.0f) {
                interlace_enabled = false;
            }

            printf("[SNES] FPS: %.2f | HEAP: %u | INTERLACE: %s | ZOOM: %d%%\n",
                   fps,
                   esp_get_free_heap_size(),
                   interlace_enabled ? "ON" : "OFF",
                   snesZoomPercent);

            frameCount = 0;
            lastFpsMs  = nowMs;
        }

        // pacing 60 Hz
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