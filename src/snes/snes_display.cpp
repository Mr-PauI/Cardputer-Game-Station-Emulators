#include "snes_display.h"

#include <Arduino.h>
#include <M5Cardputer.h>
#include "esp_heap_caps.h"
#include <string.h>

#include "snes9x/snes9x.h"

static constexpr int LCD_W = 240;
static constexpr int LCD_H = 135;

int snesZoomPercent = 100;

// ================== TRANSFORM ==================

static SnesDisplayTransform s_transform;

static int s_lastZoomPercent = -1;
static int s_lastSrcW = 0;
static int s_lastSrcH = 0;

static void snes_display_compute_transform(int srcW, int srcH)
{
    if (srcW <= 0 || srcH <= 0) {
        return;
    }

    const int zoom = (snesZoomPercent > 0) ? snesZoomPercent : 100;

    if (zoom == s_lastZoomPercent &&
        srcW == s_lastSrcW &&
        srcH == s_lastSrcH) {
        return;
    }

    s_lastZoomPercent = zoom;
    s_lastSrcW = srcW;
    s_lastSrcH = srcH;

    const float zoomFactor = (float)zoom / 100.0f;

    // destination reste fixe
    s_transform.srcW = srcW;
    s_transform.srcH = srcH;
    s_transform.dstW = LCD_W;
    s_transform.dstH = LCD_H;
    s_transform.xOffset = 0;
    s_transform.yOffset = 0;

    s_transform.invScaleX = ((float)srcW / (float)LCD_W) / zoomFactor;
    s_transform.invScaleY = ((float)srcH / (float)LCD_H) / zoomFactor;
}

extern "C" void snes_display_update_transform(int srcW, int srcH)
{
    snes_display_compute_transform(srcW, srcH);
}

extern "C" void snes_display_get_transform(SnesDisplayTransform *out)
{
    if (!out) return;
    *out = s_transform;
}

#ifndef SNES_NO_THREADED_DISPLAY

typedef struct {
    uint16_t y1;
    uint16_t y2;
    uint8_t has_second;
    uint16_t dstW;
    int16_t  xOffset;
    uint16_t pixels[LCD_W];
} SnesLineBuf;

enum BufState : uint8_t {
    BUF_FREE = 0,
    BUF_READY,
    BUF_DRAWING
};

static TaskHandle_t  s_task    = nullptr;
static volatile bool s_running = false;

static SnesLineBuf *s_buf = nullptr;
static volatile BufState s_state[2] = { BUF_FREE, BUF_FREE };

static constexpr uint32_t NOTIF_BUF0 = (1u << 0);
static constexpr uint32_t NOTIF_BUF1 = (1u << 1);

// ================== DISPLAY TASK ==================

static void snes_display_task(void *arg)
{
    (void)arg;

    M5Cardputer.Display.startWrite();

    for (;;) {
        uint32_t notif = 0;
        xTaskNotifyWait(0, 0xFFFFFFFFu, &notif, portMAX_DELAY);

        if (!s_running) {
            continue;
        }

        for (int i = 0; i < 2; ++i) {
            const uint32_t bit = (i == 0) ? NOTIF_BUF0 : NOTIF_BUF1;

            if ((notif & bit) == 0) continue;
            if (s_state[i] != BUF_READY) continue;

            s_state[i] = BUF_DRAWING;

            const int drawX = s_buf[i].xOffset;
            const int drawW = s_buf[i].dstW;

            const uint16_t y1 = s_buf[i].y1;
            if (y1 < LCD_H && drawW > 0) {
                M5Cardputer.Display.setAddrWindow(drawX, (int)y1, drawW, 1);
                M5Cardputer.Display.pushPixels(s_buf[i].pixels, drawW);
            }

            if (s_buf[i].has_second) {
                const uint16_t y2 = s_buf[i].y2;
                if (y2 < LCD_H && drawW > 0) {
                    M5Cardputer.Display.setAddrWindow(drawX, (int)y2, drawW, 1);
                    M5Cardputer.Display.pushPixels(s_buf[i].pixels, drawW);
                }
            }

            s_state[i] = BUF_FREE;
        }
    }

    M5Cardputer.Display.endWrite();
}

// ================== PUBLIC API ==================

extern "C" void snes_display_init(void)
{
    M5Cardputer.Display.setSwapBytes(true);
    M5Cardputer.Display.fillScreen(TFT_BLACK);

    if (s_buf) {
        heap_caps_free(s_buf);
        s_buf = nullptr;
    }

    s_buf = (SnesLineBuf *)heap_caps_malloc(
        sizeof(SnesLineBuf) * 2,
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT
    );

    if (!s_buf) {
        printf("[SNES-DISP] buffer alloc failed\n");
        return;
    }

    s_state[0] = BUF_FREE;
    s_state[1] = BUF_FREE;

    snes_display_compute_transform(SNES_WIDTH, SNES_HEIGHT);
}

extern "C" void snes_display_start(void)
{
    if (s_task) return;
    if (!s_buf) return;

    s_running = true;

    BaseType_t ok = xTaskCreatePinnedToCore(
        snes_display_task,
        "SnesDisp",
        1800,
        nullptr,
        6,
        &s_task,
        0
    );

    if (ok != pdPASS) {
        if (s_task) {
            vTaskDelete(s_task);
        }
        s_task = nullptr;
        s_running = false;
        printf("[SNES-DISP] task create failed\n");
    }
}

extern "C" void snes_display_stop(void)
{
    s_running = false;

    if (s_task) {
        xTaskNotify(s_task, 0, eNoAction);
        vTaskDelete(s_task);
        s_task = nullptr;
    }

    if (s_buf) {
        heap_caps_free(s_buf);
        s_buf = nullptr;
    }

    s_state[0] = BUF_FREE;
    s_state[1] = BUF_FREE;
}

extern "C" void snes_display_wait_idle(void)
{
    if (!s_running || !s_task) return;

    while (s_state[0] != BUF_FREE || s_state[1] != BUF_FREE) {
        taskYIELD();
    }
}

extern "C" void snes_display_submit_line_ex(uint32_t y1,
                                            uint32_t y2,
                                            bool has_second,
                                            const uint16_t *pixels,
                                            uint32_t width)
{
    if (!pixels || !s_running || !s_task || !s_buf) return;

    snes_display_compute_transform(SNES_WIDTH, SNES_HEIGHT);

    const int dstW = s_transform.dstW;
    const int xOffset = s_transform.xOffset;
    const float invScaleX = s_transform.invScaleX;
    const float srcCX = (float)width * 0.5f;
    const float dstCX = (float)(dstW - 1) * 0.5f;

    if (dstW <= 0 || dstW > LCD_W) return;
    if (y1 >= LCD_H && (!has_second || y2 >= LCD_H)) return;

    int idx = -1;

    if (s_state[0] == BUF_FREE) {
        idx = 0;
    } else if (s_state[1] == BUF_FREE) {
        idx = 1;
    } else {
        const uint32_t start = micros();
        while ((micros() - start) < 200) {
            if (s_state[0] == BUF_FREE) {
                idx = 0;
                break;
            }
            if (s_state[1] == BUF_FREE) {
                idx = 1;
                break;
            }
            taskYIELD();
        }

        if (idx < 0) {
            return;
        }
    }

    s_state[idx] = BUF_DRAWING;

    s_buf[idx].y1 = (uint16_t)y1;
    s_buf[idx].y2 = (uint16_t)y2;
    s_buf[idx].has_second = has_second ? 1 : 0;
    s_buf[idx].dstW = (uint16_t)dstW;
    s_buf[idx].xOffset = (int16_t)xOffset;

    for (int x = 0; x < dstW; ++x) {
        float srcXf = srcCX + ((float)x - dstCX) * invScaleX;
        int srcX = (int)srcXf;
        if (srcX < 0) srcX = 0;
        if (srcX >= (int)width) srcX = (int)width - 1;
        s_buf[idx].pixels[x] = pixels[srcX];
    }

    s_state[idx] = BUF_READY;
    xTaskNotify(s_task, (idx == 0) ? NOTIF_BUF0 : NOTIF_BUF1, eSetBits);
}

extern "C" void snes_display_submit_line(uint32_t y,
                                         const uint16_t *pixels,
                                         uint32_t width)
{
    snes_display_submit_line_ex(y, 0, false, pixels, width);
}

#else

extern "C" void snes_display_init(void)
{
    M5Cardputer.Display.setSwapBytes(true);
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    snes_display_compute_transform(SNES_WIDTH, SNES_HEIGHT);
}

extern "C" void snes_display_start(void)
{
    M5Cardputer.Display.startWrite();
}

extern "C" void snes_display_stop(void)
{
    M5Cardputer.Display.endWrite();
}

extern "C" void snes_display_wait_idle(void)
{
}

extern "C" void snes_display_submit_line_ex(uint32_t y1,
                                            uint32_t y2,
                                            bool has_second,
                                            const uint16_t *pixels,
                                            uint32_t width)
{
    if (!pixels) return;

    snes_display_compute_transform(SNES_WIDTH, SNES_HEIGHT);

    const int dstW = s_transform.dstW;
    const int xOffset = s_transform.xOffset;
    const float invScaleX = s_transform.invScaleX;
    const float srcCX = (float)width * 0.5f;
    const float dstCX = (float)(dstW - 1) * 0.5f;

    if (dstW <= 0 || dstW > LCD_W) return;

    static uint16_t lineBuf[LCD_W];

    for (int x = 0; x < dstW; ++x) {
        float srcXf = srcCX + ((float)x - dstCX) * invScaleX;
        int srcX = (int)srcXf;
        if (srcX < 0) srcX = 0;
        if (srcX >= (int)width) srcX = (int)width - 1;
        lineBuf[x] = pixels[srcX];
    }

    if (y1 < LCD_H) {
        M5Cardputer.Display.setAddrWindow(xOffset, (int)y1, dstW, 1);
        M5Cardputer.Display.pushPixels(lineBuf, dstW);
    }

    if (has_second && y2 < LCD_H) {
        M5Cardputer.Display.setAddrWindow(xOffset, (int)y2, dstW, 1);
        M5Cardputer.Display.pushPixels(lineBuf, dstW);
    }
}

extern "C" void snes_display_submit_line(uint32_t y,
                                         const uint16_t *pixels,
                                         uint32_t width)
{
    snes_display_submit_line_ex(y, 0, false, pixels, width);
}

#endif