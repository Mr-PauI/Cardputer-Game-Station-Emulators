#pragma GCC optimize ("Os")

#include "input.h"
#include <algorithm>
#include <M5Cardputer.h>
#include "share/input.h"

extern bool fullscreen;
extern bool scanline;
extern int smsZoomPercent;

static inline bool key(char c) {
    return M5Cardputer.Keyboard.isKeyPressed(c);
}

void cardputer_input_init() {
}

void cardputer_read_input(bool isGG) {
    int smsButtons = 0; // -> input.pad[0]
    int smsSystem  = 0; // -> input.system

    M5Cardputer.update();
    Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

    share::checkCommonInput(status);

    // I2C PAD (M5Stack JoyV2)
    if (share::hasI2cPad()) {
        int i2cPad = share::pollI2cPad();
        if (i2cPad & share::PAD_LEFT)  smsButtons |= INPUT_LEFT;
        if (i2cPad & share::PAD_RIGHT) smsButtons |= INPUT_RIGHT;
        if (i2cPad & share::PAD_UP)    smsButtons |= INPUT_UP;
        if (i2cPad & share::PAD_DOWN)  smsButtons |= INPUT_DOWN;
        if (i2cPad & share::PAD_A)     smsButtons |= INPUT_BUTTON1;
    }

    // --- fullscreen / zoom cycle ---
    if (M5Cardputer.Keyboard.isChange() && key(CARDPUTER_SCREEN_TOGGLE)) {
        if (!fullscreen) {
            fullscreen = true;
            scanline   = true;
            smsZoomPercent = 100;
        } else {
            smsZoomPercent += 10;
            if (smsZoomPercent > 150) {
                smsZoomPercent = 100;
                fullscreen = false;
                scanline   = false;
            }
        }

        // Recalculer le scaler immédiatement
        if (fullscreen) video_compute_scaler_full();
        else            video_compute_scaler_square();

        input.pad[0] = 0; input.system = 0;
        return;
    }

    // --- Zoom fin (FN + , ou FN + /) ---
    if (status.fn && key(CARDPUTER_ZOOM_PLUS)) {
        smsZoomPercent += 1;
        if (smsZoomPercent > 150) smsZoomPercent = 150;
        video_compute_scaler_full();
        input.pad[0] = 0; input.system = 0;
        return;
    }

    if (status.fn && key(CARDPUTER_ZOOM_MINUS)) {
        smsZoomPercent -= 1;
        if (smsZoomPercent < 100) smsZoomPercent = 100;
        video_compute_scaler_full();
        input.pad[0] = 0; input.system = 0;
        return;
    }
    
    // ---------- Mapping  ----------
    if (key(CARDPUTER_LEFT_1) || key(CARDPUTER_LEFT_2)) smsButtons |= INPUT_LEFT;
    if (key(CARDPUTER_RIGHT_1) || key(CARDPUTER_RIGHT_2)) smsButtons |= INPUT_RIGHT;
    if (key(CARDPUTER_UP_1) || key(CARDPUTER_UP_2)) smsButtons |= INPUT_UP;
    if (key(CARDPUTER_DOWN_1) || key(CARDPUTER_DOWN_2) || key(CARDPUTER_DOWN_3)) smsButtons |= INPUT_DOWN;
    if (key(CARDPUTER_BTN_A_1) || key(CARDPUTER_BTN_A_2)) smsButtons |= INPUT_BUTTON1;
    if (key(CARDPUTER_BTN_B))             smsButtons |= INPUT_BUTTON2;

    if (isGG) {
        if (key(CARDPUTER_BTN_START)) smsSystem |= INPUT_START;
    } else {
        if (key(CARDPUTER_BTN_START)) smsSystem |= INPUT_PAUSE;
    }

    input.pad[0]  = smsButtons;
    input.system  = smsSystem;
}
