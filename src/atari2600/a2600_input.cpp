#pragma GCC optimize ("Os")

#include "a2600_input.h"
#include "Event.hxx"
#include "a2600_display.h"
#include "share/input.h"

void a2600_input_init(void)
{
}

void a2600_input_update(Event& event)
{
    M5Cardputer.update();
    Keyboard_Class::KeysState ks = M5Cardputer.Keyboard.keysState();

    share::checkCommonInput(ks);

    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool fire = false;
    bool select = false;
    bool reset = false;

    // I2C PAD
    if (share::hasI2cPad()) {
        const uint32_t pad = share::pollI2cPad();
        if (pad & share::PAD_LEFT)  left = true;
        if (pad & share::PAD_RIGHT) right = true;
        if (pad & share::PAD_UP)    up = true;
        if (pad & share::PAD_DOWN)  down = true;
        if (pad & share::PAD_A)     fire = true;
    }

    // ================== SCREEN MODE ==================
    if (M5Cardputer.Keyboard.isChange() &&
        M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_SCREEN_TOGGLE)) {
        if (!a2600FullScreen) {
            a2600FullScreen = true;
            a2600ZoomPercent = 100;
        } else {
            a2600ZoomPercent += 10;
            if (a2600ZoomPercent > 150) {
                a2600ZoomPercent = 100;
                a2600FullScreen = false;
            }
        }
    }

    // ================== DIRECTIONS ==================
    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_LEFT_1) ||
        M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_LEFT_2)) {
        left = true;
    }

    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_RIGHT_1) ||
        M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_RIGHT_2)) {
        right = true;
    }

    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_UP_1) ||
        M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_UP_2)) {
        up = true;
    }

    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_DOWN_1) ||
        M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_DOWN_2) ||
        M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_DOWN_3)) {
        down = true;
    }

    // ================== BOUTONS ATARI ==================
    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_BTN_A_1) ||
        M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_BTN_A_2) ||
        M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_BTN_B)) {
        fire = true;
    }

    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_BTN_SELECT)) {
        select = true;
    }

    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_BTN_START)) {
        reset = true;
    }

    // ================== ZOOM ==================
    if (ks.fn && M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_ZOOM_PLUS)) {
        if (!a2600FullScreen) {
            a2600FullScreen = true;
        }
        if (a2600ZoomPercent < 150) {
            a2600ZoomPercent++;
        }
    }

    if (ks.fn && M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_ZOOM_MINUS)) {
        if (!a2600FullScreen) {
            a2600FullScreen = true;
        }
        if (a2600ZoomPercent > 100) {
            a2600ZoomPercent--;
        }
    }

    event.set(Event::Type(Event::JoystickZeroUp),    up ? 1 : 0);
    event.set(Event::Type(Event::JoystickZeroDown),  down ? 1 : 0);
    event.set(Event::Type(Event::JoystickZeroLeft),  left ? 1 : 0);
    event.set(Event::Type(Event::JoystickZeroRight), right ? 1 : 0);
    event.set(Event::Type(Event::JoystickZeroFire),  fire ? 1 : 0);
    event.set(Event::Type(Event::ConsoleSelect),     select ? 1 : 0);
    event.set(Event::Type(Event::ConsoleReset),      reset ? 1 : 0);
}