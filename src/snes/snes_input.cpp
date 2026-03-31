#include "snes_input.h"

#include <M5Cardputer.h>
#include <Arduino.h>
#include "share/input.h"
#include "esp_heap_caps.h"

extern "C" {
    #include "snes9x/snes9x.h"
}

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern int snesZoomPercent;

static volatile uint32_t s_lastInputMask = 0; 

#ifndef SNES_NO_THREADED_INPUT

// -----------------------------------------------------------------------------
// State
// -----------------------------------------------------------------------------

static TaskHandle_t s_inputTaskHandle = nullptr;
static volatile uint32_t s_inputMask  = 0;

// -----------------------------------------------------------------------------
// Computes the current SNES button mask
// -----------------------------------------------------------------------------

uint32_t snes_input_compute_mask()
{
    uint32_t mask = 0;

    if (share::shouldPollInput() == false) {
        return s_lastInputMask;
    }

    M5Cardputer.update();
    Keyboard_Class::KeysState ks = M5Cardputer.Keyboard.keysState();

    // vol, bright, quit, etc.
    share::checkCommonInput(ks);

    // ================== ZOOM ==================

    if (M5Cardputer.Keyboard.isChange() &&
        M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_SCREEN_TOGGLE)) {
        snesZoomPercent += 10;
        if (snesZoomPercent > 150) {
            snesZoomPercent = 100;
        }
        return s_lastInputMask;
    }

    if (ks.fn && M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_ZOOM_PLUS)) {
        snesZoomPercent = (snesZoomPercent < 150) ? (snesZoomPercent + 1) : 150;
        return s_lastInputMask;
    }

    if (ks.fn && M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_ZOOM_MINUS)) {
        snesZoomPercent = (snesZoomPercent > 100) ? (snesZoomPercent - 1) : 100;
        return s_lastInputMask;
    }

    // ================== I2C PAD (M5Stack JoyV2) ==================
    if (share::hasI2cPad()) {
        int i2cPad = share::pollI2cPad();

        if (i2cPad & share::PAD_LEFT)  mask |= SNES_LEFT_MASK;
        if (i2cPad & share::PAD_RIGHT) mask |= SNES_RIGHT_MASK;
        if (i2cPad & share::PAD_UP)    mask |= SNES_UP_MASK;
        if (i2cPad & share::PAD_DOWN)  mask |= SNES_DOWN_MASK;
        if (i2cPad & share::PAD_A)     mask |= SNES_B_MASK;
    }

    // ================== DIRECTIONS (keyboard Cardputer) ==================
    // Left : 'a' ou ','
    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_LEFT_1) ||
        M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_LEFT_2)) {
        mask |= SNES_LEFT_MASK;
    }

    // Right : 'd' ou '/'
    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_RIGHT_1) ||
        M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_RIGHT_2)) {
        mask |= SNES_RIGHT_MASK;
    }

    // Up : 'e' ou ';'
    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_UP_1) ||
        M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_UP_2)) {
        mask |= SNES_UP_MASK;
    }

    // Down : 's', '.' ou 'z'
    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_DOWN_1) ||
        M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_DOWN_2) ||
        M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_DOWN_3)) {
        mask |= SNES_DOWN_MASK;
    }

    // ================== BOUTONS SNES ==================

    // B SNES
    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_BTN_B)) {
        mask |= SNES_B_MASK;
    }

    // Y SNES
    if (M5Cardputer.Keyboard.isKeyPressed('o')) {
        mask |= SNES_Y_MASK;
    }

    // X SNES : touche 'i'
    if (M5Cardputer.Keyboard.isKeyPressed('p')) {
        mask |= SNES_X_MASK;
    }

    // A SNES : touche 'o'
    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_BTN_A_1)) {
        mask |= SNES_A_MASK;
    }

    // L SNES : touche 'u'
    if (M5Cardputer.Keyboard.isKeyPressed('i')) {
        mask |= SNES_TL_MASK;
    }

    // R SNES : touche 'p'
    if (M5Cardputer.Keyboard.isKeyPressed('j')) {
        mask |= SNES_TR_MASK;
    }

    // START
    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_BTN_START)) {
        mask |= SNES_START_MASK;
    }

    // SELECT
    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_BTN_SELECT)) {
        mask |= SNES_SELECT_MASK;
    }

    s_lastInputMask = mask;
    return mask;
}

// -----------------------------------------------------------------------------
// Task FreeRTOS
// -----------------------------------------------------------------------------

static void snes_input_task(void *arg)
{
    (void)arg;

    for (;;) {
        s_inputMask = snes_input_compute_mask();
        // 30 hz 
        vTaskDelay(pdMS_TO_TICKS(32));
    }
}

// -----------------------------------------------------------------------------
// API C
// -----------------------------------------------------------------------------

extern "C" void snes_input_start(void)
{
    if (s_inputTaskHandle != nullptr)
        return;

    xTaskCreatePinnedToCore(
        snes_input_task,     // task function
        "snes_input",         // name
        2048,                 // stack size
        nullptr,              // param
        0,                    // priority
        &s_inputTaskHandle,   // handle
        0                     // CORE
    );
}

extern "C" void snes_input_stop(void)
{
    if (s_inputTaskHandle == nullptr)
        return;

    vTaskDelete(s_inputTaskHandle);
    s_inputTaskHandle = nullptr;
    s_inputMask       = 0;
}

// Used by S9xReadJoypad
extern "C" uint32_t snes_input_poll(void)
{
    return s_inputMask;
}

#else

void snes_input_start()
{
    // nothing
}

void snes_input_stop()
{
    // nothing
}

uint32_t snes_input_poll()
{
    uint32_t mask = 0;

    if (share::shouldPollInput() == false) {
        return s_lastInputMask;
    }

    M5Cardputer.update();
    Keyboard_Class::KeysState ks = M5Cardputer.Keyboard.keysState();

    // vol, bright, quit, etc.
    share::checkCommonInput(ks);

    // ================== ZOOM ==================

    if (M5Cardputer.Keyboard.isChange() &&
        M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_SCREEN_TOGGLE)) {
        snesZoomPercent += 10;
        if (snesZoomPercent > 150) {
            snesZoomPercent = 100;
        }
        return s_lastInputMask;
    }

    if (ks.fn && M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_ZOOM_PLUS)) {
        snesZoomPercent = (snesZoomPercent < 150) ? (snesZoomPercent + 1) : 150;
        return s_lastInputMask;
    }

    if (ks.fn && M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_ZOOM_MINUS)) {
        snesZoomPercent = (snesZoomPercent > 100) ? (snesZoomPercent - 1) : 100;
        return s_lastInputMask;
    }

    // // ================== I2C PAD (M5Stack JoyV2) ==================
    if (share::hasI2cPad()) {
        int i2cPad = share::pollI2cPad();

        if (i2cPad & share::PAD_LEFT)  mask |= SNES_LEFT_MASK;
        if (i2cPad & share::PAD_RIGHT) mask |= SNES_RIGHT_MASK;
        if (i2cPad & share::PAD_UP)    mask |= SNES_UP_MASK;
        if (i2cPad & share::PAD_DOWN)  mask |= SNES_DOWN_MASK;
        if (i2cPad & share::PAD_A)     mask |= SNES_B_MASK;
    }

    // ================== DIRECTIONS (keyboard Cardputer) ==================
    // Left : 'a' ou ','
    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_LEFT_1) ||
        M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_LEFT_2)) {
        mask |= SNES_LEFT_MASK;
    }

    // Right : 'd' ou '/'
    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_RIGHT_1) ||
        M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_RIGHT_2)) {
        mask |= SNES_RIGHT_MASK;
    }

    // Up : 'e' ou ';'
    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_UP_1) ||
        M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_UP_2)) {
        mask |= SNES_UP_MASK;
    }

    // Down : 's', '.' ou 'z'
    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_DOWN_1) ||
        M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_DOWN_2) ||
        M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_DOWN_3)) {
        mask |= SNES_DOWN_MASK;
    }

    // ================== BOUTONS SNES ==================

    // B SNES
    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_BTN_B)) {
        mask |= SNES_B_MASK;
    }

    // Y SNES
    if (M5Cardputer.Keyboard.isKeyPressed('o')) {
        mask |= SNES_Y_MASK;
    }

    // X SNES : touche 'i'
    if (M5Cardputer.Keyboard.isKeyPressed('p')) {
        mask |= SNES_X_MASK;
    }

    // A SNES : touche 'o'
    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_BTN_A_1)) {
        mask |= SNES_A_MASK;
    }

    // L SNES : touche 'u'
    if (M5Cardputer.Keyboard.isKeyPressed('i')) {
        mask |= SNES_TL_MASK;
    }

    // R SNES : touche 'p'
    if (M5Cardputer.Keyboard.isKeyPressed('j')) {
        mask |= SNES_TR_MASK;
    }

    // START
    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_BTN_START)) {
        mask |= SNES_START_MASK;
    }

    // SELECT
    if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_BTN_SELECT)) {
        mask |= SNES_SELECT_MASK;
    }

    s_lastInputMask = mask;
    return mask;
}

#endif