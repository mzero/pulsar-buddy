#include "critical.h"

#include <Arduino.h>

#include "controls.h"
#include "display.h"


namespace {
  bool criticalOnDisplay = false;
  bool criticalNeedsRedisplay = false;

  bool writeOnDisplay() {
    if (Serial.getWriteError()) {
      Serial.clearWriteError();

      if (!criticalOnDisplay) {
        criticalOnDisplay = true;
        display.clearDisplay();
        display.setTextColor(WHITE);
        display.setFont();
        display.setTextWrap(true);
        display.setCursor(0, 0);

        display.println("====[ Critical\x13 ]====");
      }

      criticalNeedsRedisplay = true;
      return true;
    }
    return false;
  }

}

size_t Critical::write(uint8_t c) {
  size_t r;

  Serial.clearWriteError();
  r = Serial.write(c);
  if (writeOnDisplay()) {
    r = display.write(c);
  }

  return r;
}

size_t Critical::write(const uint8_t *buffer, size_t size) {
  size_t r;

  Serial.clearWriteError();
  r = Serial.write(buffer, size);
  if (writeOnDisplay()) {
    r = display.write(buffer, size);
  }

  return r;
}


Critical::State Critical::update() {
  if (!criticalOnDisplay)
    return inactive;

  if (criticalNeedsRedisplay)
    display.display();

  if (encoderButton.update() == Button::Down) {
    resetText();
    criticalOnDisplay = false;
    criticalNeedsRedisplay = false;
    return closed;
  }

  return active;
}


Critical critical;
