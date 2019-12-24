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
  Serial.clearWriteError();
  Serial.write(c);
  if (writeOnDisplay()) {
    display.write(c);
  }
}

size_t Critical::write(const uint8_t *buffer, size_t size) {
  Serial.clearWriteError();
  Serial.write(buffer, size);
  if (writeOnDisplay()) {
    display.write(buffer, size);
  }
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
