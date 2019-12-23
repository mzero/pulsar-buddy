#include "config.h"

#include "controls.h"
#include "display.h"
#include "flash.h"

Configuration configuration;

namespace {

  FlashLog<Configuration> configurationLog;

  int selectedField = 0;
  const int minField = 0;
  const int maxField = 7;

  bool clickSelectedField() {
    switch (selectedField) {
      case 0:
        configurationLog.save(configuration);
        return true;

      case 1: configuration.screen.alwaysDim      ^= 1; break;
      case 2: configuration.screen.saverDisable   ^= 1; break;
      case 3: configuration.debug.waitForSerial   ^= 1; break;
      case 4: configuration.debug.flash           ^= 1; break;
      case 5: configuration.debug.timing          ^= 1; break;
      case 6: configuration.debug.clock           ^= 1; break;
      case 7: configuration.debug.plotClock       ^= 1; break;
      default: ;
    }
    return false;
  }

  void drawButton(const char* s, int field) {
    if (selectedField == field)
      display.setTextColor(BLACK, WHITE);

    display.print(s);
    display.setTextColor(WHITE, BLACK);
  }

  void drawFlag(const char* s, bool f, int field) {
    display.print(' ');
    if (selectedField == field)
      display.setTextColor(BLACK, WHITE);
    display.print(f ? '+' : '-');
    display.print(s);
    display.setTextColor(WHITE, BLACK);
  }

  void drawConfiguration() {
    display.clearDisplay();
    display.setTextColor(WHITE, BLACK);

    // top line
    display.setCursor(0, 0);
    drawButton("\x1b", 0);
    display.print(" Pulsar Buddy v. 001");

    // screen line
    display.setCursor(0, 8);
    display.print("Screen:");
    drawFlag("dim", configuration.screen.alwaysDim, 1);
    drawFlag("saver", !configuration.screen.saverDisable, 2);

    // debug line 1
    display.setCursor(0, 16);
    display.print("Debug:");
    drawFlag("wait", configuration.debug.waitForSerial, 3);
    drawFlag("flash", configuration.debug.flash, 4);

    // debug line 2
    display.setCursor(0, 24);
    drawFlag("time", configuration.debug.timing, 5);
    drawFlag("clk", configuration.debug.clock, 6);
    drawFlag("plot", configuration.debug.plotClock, 7);

    display.display();
  }

  void configurationLoop() {
    bool redraw = true;

    while (true) {
      if (redraw) {
        drawConfiguration();
        redraw = false;
      }

      auto update = encoder.update();
      if (update.active()) {
        selectedField =
          constrain(selectedField + update.dir(), minField, maxField);
        redraw = true;
      }

      auto s = encoderButton.update();
      if (s == Button::Down) {
        if (clickSelectedField())
          return;
        redraw = true;
      }

      yield();
    }
  }
}

void Configuration::initialize() {
  configurationLog.begin(16,8);

  if (!configurationLog.load(configuration)) {
    memset(&configuration, 0, sizeof(configuration));
  }

  encoderButton.update();
  delay(60); // enough time to have button debounce
  encoderButton.update();
  if (encoderButton.active()) {
    configurationLoop();
  }
}
