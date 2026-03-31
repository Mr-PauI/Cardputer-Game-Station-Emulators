#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int snesZoomPercent;

typedef struct {
    int   srcW;
    int   srcH;
    int   dstW;
    int   dstH;
    int   xOffset;
    int   yOffset;
    float invScaleX;
    float invScaleY;
} SnesDisplayTransform;

void snes_display_init(void);
void snes_display_start(void);
void snes_display_stop(void);
void snes_display_wait_idle(void);

void snes_display_update_transform(int srcW, int srcH);
void snes_display_get_transform(SnesDisplayTransform *out);

void snes_display_submit_line(uint32_t y,
                              const uint16_t *pixels,
                              uint32_t width);

void snes_display_submit_line_ex(uint32_t y1,
                                 uint32_t y2,
                                 bool has_second,
                                 const uint16_t *pixels,
                                 uint32_t width);

#ifdef __cplusplus
}
#endif