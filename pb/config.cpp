#include "config.h"

#include "controls.h"
#include "display.h"
#include "flash.h"
#include "version.h"

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

      case 1: configuration.options.extendedBpmRange ^= 1; break;
      case 2: configuration.options.alwaysDim        ^= 1; break;
      case 3: configuration.options.saverDisable     ^= 1; break;
      case 4: configuration.debug.waitForSerial     ^= 1; break;
      case 5: configuration.debug.flash             ^= 1; break;
      case 6: configuration.debug.timing            ^= 1; break;
      case 7: configuration.debug.plotClock         ^= 1; break;
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
    display.print(" Pulsar Buddy v. ");
    display.print(PB_VERSION);

    // options line
    display.setCursor(0, 8);
    display.print("Opts:");
    drawFlag("extBPM", configuration.options.extendedBpmRange, 1);

    // screen line
    display.setCursor(0, 16);
    display.print("Screen:");
    drawFlag("dim", configuration.options.alwaysDim, 2);
    drawFlag("saver", !configuration.options.saverDisable, 3);

    // debug line
    display.setCursor(0, 24);
    display.print("Debug:");
    drawFlag("wait", configuration.debug.waitForSerial, 4);
    drawFlag("f", configuration.debug.flash, 5);
    drawFlag("t", configuration.debug.timing, 6);
    drawFlag("p", configuration.debug.plotClock, 7);

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
