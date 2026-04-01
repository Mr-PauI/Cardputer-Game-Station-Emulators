#pragma once

enum SnesScreenMode {
    SNES_SCREEN_INTERLACE,
    SNES_SCREEN_LINE_DUPLICATE
};

extern volatile SnesScreenMode g_snesScreenMode;