#pragma GCC optimize ("Os")

#include "pce_input.h"

#include <M5Cardputer.h>
#include "share/input.h"

extern "C" {
  #include "pce-go/pce.h" 
}

extern bool pceFullScreen;
extern int  pceZoomLevel;

void pce_input_read(uint8_t joypads[8])
{
  for (int i = 0; i < 8; ++i) {
    joypads[i] = 0;
  }

  uint32_t buttons = 0;

  M5Cardputer.update();
  Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

  share::checkCommonInput(status);

  // I2C PAD (M5Stack JoyV2)
  if (share::hasI2cPad()) {
      int i2cPad = share::pollI2cPad();
      if (i2cPad & share::PAD_LEFT)  buttons |= JOY_LEFT;
      if (i2cPad & share::PAD_RIGHT) buttons |= JOY_RIGHT;
      if (i2cPad & share::PAD_UP)    buttons |= JOY_UP;
      if (i2cPad & share::PAD_DOWN)  buttons |= JOY_DOWN;
      if (i2cPad & share::PAD_A)     buttons |= JOY_A;
  }

  // Toggle / fullscreen
  if (M5Cardputer.Keyboard.isChange() &&
      M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_SCREEN_TOGGLE)) {

    if (!pceFullScreen) {
      pceFullScreen  = true;
      pceZoomLevel  = 100;
    } else {
      pceZoomLevel += 10;
      if (pceZoomLevel > 150) {
        pceZoomLevel = 100;
        pceFullScreen = false;
      }
    }
    joypads[0] = 0;
    return;
  }

  // Zoom in
  if (status.fn && M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_ZOOM_PLUS)) {
    if (!pceFullScreen) pceFullScreen = true;
    pceZoomLevel += 1;
    if (pceZoomLevel > 150) pceZoomLevel = 150;
    joypads[0] = 0;
    return;
  }

  // Zoom out 
  if (status.fn && M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_ZOOM_MINUS)) {
    if (!pceFullScreen) pceFullScreen = true;
    pceZoomLevel -= 1;
    if (pceZoomLevel < 100) pceZoomLevel = 100;
    joypads[0] = 0;
    return;
  }

  // Gauche
  if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_LEFT_1) ||
      M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_LEFT_2)) {
    buttons |= JOY_LEFT;
  }

  // Droite
  if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_RIGHT_1) ||
      M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_RIGHT_2)) {
    buttons |= JOY_RIGHT;
  }

  // Haut
  if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_UP_1) ||
      M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_UP_2)) {
    buttons |= JOY_UP;
  }

  // Bas
  if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_DOWN_1) ||
      M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_DOWN_2) ||
      M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_DOWN_3)) {
    buttons |= JOY_DOWN;
  }

  // Select
  if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_BTN_SELECT)) {
    buttons |= JOY_SELECT;
  }

  // Start
  if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_BTN_START)) {
    buttons |= JOY_RUN;
  }

  // Bouton I (A)
  if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_BTN_A_1) ||
      M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_BTN_A_2)) {
    buttons |= JOY_A;
  }

  // Bouton II (B)
  if (M5Cardputer.Keyboard.isKeyPressed(CARDPUTER_BTN_B)) {
    buttons |= JOY_B;
  }

  joypads[0] = (uint8_t)buttons;
}

// ================== HOOKS PCE-GO ==================

extern "C" void osd_input_read(uint8_t joypads[8])
{
  pce_input_read(joypads);
}