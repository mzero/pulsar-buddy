#include "config.h"

#include "controls.h"
#include "display.h"
#include "flash.h"
#include "flash_reset.h"
#include "test.h"
#include "version.h"

Configuration configuration;

namespace {

  FlashLog<Configuration> configurationLog;

  enum FieldId {
      fidBegin = 0,

      fidSaveAndExit = fidBegin,

      fidExtendedBpmRange,
      fidAlwaysDim,
      fidSaverDisable,
      fidFlipDisplay,
      fidWaitForSerial,
      fidFlash,
      fidTiming,
      fidPlotClock,

      fidHardwareTest,
      fidFlashTestAndReset,

      fidEnd
  };

  FieldId selectedField = fidSaveAndExit;


  bool clickSelectedField() {
    switch (selectedField) {
      case fidSaveAndExit:
        configurationLog.save(configuration);
        return true;

      case fidExtendedBpmRange: configuration.options.extendedBpmRange ^= 1; break;
      case fidAlwaysDim:        configuration.options.alwaysDim        ^= 1; break;
      case fidSaverDisable:     configuration.options.saverDisable     ^= 1; break;
      case fidFlipDisplay:      configuration.options.flipDisplay      ^= 1; break;
      case fidWaitForSerial:    configuration.debug.waitForSerial     ^= 1; break;
      case fidFlash:            configuration.debug.flash             ^= 1; break;
      case fidTiming:           configuration.debug.timing            ^= 1; break;
      case fidPlotClock:        configuration.debug.plotClock         ^= 1; break;

      case fidHardwareTest:
        testLoop();
        break;

      case fidFlashTestAndReset:
        flashTestAndReset();
        break;

      default: ;
    }
    return false;
  }

  void drawButton(const char* s, FieldId field) {
    if (selectedField == field)
      display.setTextColor(BLACK, WHITE);

    display.print(s);
    display.setTextColor(WHITE, BLACK);
  }

  void drawFlag(const char* s, bool f, FieldId field) {
    if (selectedField == field)
      display.setTextColor(BLACK, WHITE);
    display.print(f ? '+' : '-');
    display.print(s);
    display.setTextColor(WHITE, BLACK);
  }

  void drawConfiguration() {
    setRotationNormal();
    display.clearDisplay();
    display.setTextColor(WHITE, BLACK);

    // top line
    display.setCursor(0, 0);
    drawButton("\x1b", fidSaveAndExit);
#ifndef PB_DEVELOPMENT
    display.print(" Pulsar Buddy v. ");
#else
    display.print(" Pulsar Buddy dev");
#endif
    display.print(PB_VERSION);

    // options line
    display.setCursor(0, 8);
    display.print("Opts: ");
    drawFlag("extBPM", configuration.options.extendedBpmRange, fidExtendedBpmRange);

    // screen line
    display.setCursor(0, 16);
    display.print("Screen: ");
    drawFlag("dim", configuration.options.alwaysDim, fidAlwaysDim);
    drawFlag("sav", !configuration.options.saverDisable, fidSaverDisable);
    drawFlag("flip", configuration.options.flipDisplay, fidFlipDisplay);

    // debug line
    display.setCursor(0, 24);
    display.print("Debug: ");
    drawFlag("w", configuration.debug.waitForSerial, fidWaitForSerial);
    drawFlag("f", configuration.debug.flash, fidFlash);
    drawFlag("t", configuration.debug.timing, fidTiming);
    drawFlag("p", configuration.debug.plotClock, fidPlotClock);
    display.print(" ");
    drawButton("hw", fidHardwareTest);
    display.print(" ");
    drawButton("xx", fidFlashTestAndReset);

    display.display();
  }

  void configurationLoop() {
    bool redraw = true;

    while (true) {
      if (redraw) {
        drawConfiguration();
        redraw = false;

#ifdef SCREENSHOT_CONFIG_SCREEN
        static bool firstTime = true;
        if (firstTime) {
          while (!Serial);
          dumpDisplayPBM(Serial);
          firstTime = false;
        }
#endif
      }

      auto update = encoder.update();
      if (update.active()) {
        selectedField =
          static_cast<FieldId>(
            constrain(selectedField + update.dir(), fidBegin, fidEnd-1));
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
