#pragma GCC optimize ("Os")

#include <M5Cardputer.h>
#include "share/input.h"

extern bool fullscreenMode;
extern int nesZoomPercent;

extern "C" {

void controller_init() {
  // nothing to do
}

uint32_t controller_read_input() {
    uint32_t value = 0xFFFFFFFF;
    M5Cardputer.update();
    Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

    share::checkCommonInput(status);

    // I2C PAD (M5Stack JoyV2)
    if (share::hasI2cPad()) {
        int i2cPad = share::pollI2cPad();
        if (i2cPad & share::PAD_LEFT)  value ^= (1 << 2); // left
        if (i2cPad & share::PAD_RIGHT) value ^= (1 << 3); // right
        if (i2cPad & share::PAD_UP)    value ^= (1 << 0); // up
        if (i2cPad & share::PAD_DOWN)  value ^= (1 << 1); // down
        if (i2cPad & share::PAD_A)     value ^= (1 << 6); // A
    }
    
    // Zoom control and screen mode toggle
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_SCREEN_TOGGLE)) {
        if (!fullscreenMode) {
            fullscreenMode = true;
            nesZoomPercent = 100;
        } else {
            nesZoomPercent += 10;
            if (nesZoomPercent > 150) {
                nesZoomPercent = 100;
                fullscreenMode = false;
            }
        }
        return value;
    }

    // Zoom in / out
    if (status.fn && M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_ZOOM_PLUS)) {
        if (!fullscreenMode) fullscreenMode = true;
        nesZoomPercent+= 1;
        if (nesZoomPercent > 150) nesZoomPercent = 150;
        return value;

    }

    if (status.fn && M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_ZOOM_MINUS)) {
        if (!fullscreenMode) fullscreenMode = true;
        nesZoomPercent-= 1;
        if (nesZoomPercent < 100) nesZoomPercent = 100;
        return value;
    }
 
    // Arrows and buttons
    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_LEFT_1) || M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_LEFT_2)) {
        value ^= (1 << 2); // left
    }
    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_RIGHT_1) || M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_RIGHT_2)) {
        value ^= (1 << 3); // right
    }

    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_UP_1) || M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_UP_2)) {
        value ^= (1 << 0); // up
    }
    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_DOWN_1) || M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_DOWN_2) || M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_DOWN_3)) {
        value ^= (1 << 1); // down
    }

    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_BTN_SELECT)) {
        value ^= (1 << 4); // select
    }

    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_BTN_START)) {
        value ^= (1 << 5); // start
    }

    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_BTN_A_1) || M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_BTN_A_2)) {
        value ^= (1 << 6); // A
    }
    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_BTN_B)) {
        value ^= (1 << 7); // B
    }

    return value;
}

} // extern "C"
