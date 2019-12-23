// #include <ZeroRegs.h>

#if 0
#include <usbmidi.h>
#endif

#include "clock.h"
#include "config.h"
#include "controls.h"
#include "display.h"
#include "layout.h"
#include "state.h"
#include "timer_hw.h"


#ifdef ZERO_REGS_H
ZeroRegOptions zeroOpts = { Serial, true };
#endif

volatile bool measureEvent = false;

void isrMeasure() {
  measureEvent = true;
}


extern "C" char* sbrk(int incr);

uint32_t sramUsed() {
  return (uint32_t)(sbrk(0)) - 0x20000000;
}


void setup() {
  auto s0 = sramUsed();

  Serial.begin(115200);
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32

  Configuration::initialize();

  if (configuration.debug.waitForSerial) {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.print("Waiting for\nserial connection...");
    display.display();

    while (!Serial);
       // wait for native usb, there is a delay in that call
       // so no need for one here
       // FIXME: Take this out for production

  }

  initializeState();
  initializeTimers();

  setBpm(userState().userBpm);
  setSync(userState().syncMode);
  resetTiming(userState().settings);

  if (configuration.screen.alwaysDim)
    display.dim(true);

  drawAll(true);
  updateSaver(true);
  selectionTimeout.activity();
  dimTimeout.activity();

  auto s1 = sramUsed();
  Serial.printf("sram used: %d static, %d post-init\n", s0, s1);
}


void loop() {
  bool active = false;

  if (measureEvent) {
    measureEvent = false;
    if (pendingState()) {
      updateTiming(userSettings());
      commitState();
      active = true;
    }
  } else {
    auto status = ClockStatus::current();
    if (status.running() && userState().userBpm != status.bpm) {
      userState().userBpm = status.bpm;
      active = true;
    }
    persistState();
  }

#if 0
  USBMIDI.poll();
  while (USBMIDI.available()) {
    auto b = USBMIDI.read();

    if (b == 0xf8) {
      midiClock();

      static bool reported = false;
      if (!reported) {
        Serial.println("MIDI clock detected");
        reported = true;
      }
    }
  }
#endif

  auto update = encoder.update();
  if (update.active()) {
    updateSelection(update);
    active = true;
  }

  Button::State s = encoderButton.update();
  if (s != Button::NoChange) {
    clickSelection(s);
    active = true;
  }

  if (encoderButton.active()) {
    active = true;
  }

  if (oledButtonA.update() == Button::Down) {
    resetTiming(userSettings());
    active = true;
  }

  if (oledButtonB.update() == Button::Down) {
#ifdef ZERO_REGS_H
    printZeroRegs(zeroOpts);
#endif
    active = true;
  }

  if (oledButtonC.update() == Button::Down) {
    Serial.println("-------------------------------------------------------");
    dumpTimers();
    dumpClock();
    active = true;
  }

  static unsigned long nextDraw = 0;
  static bool needsDraw = false;

  if (active) {
    selectionTimeout.activity();
    dimTimeout.activity();
    needsDraw = true;
    if (!configuration.screen.alwaysDim)
      display.dim(false);
  } else {
    if (selectionTimeout.update()) {
      resetSelection();
      needsDraw = true;
    }
    if (dimTimeout.update()) {
      display.dim(true);
    }
  }

  auto now = millis();
  static bool saverDrawn = false;

  if (needsDraw || now > nextDraw) {
    bool drew = drawAll(false);
    needsDraw = false;
    nextDraw = now + 50;  // redraw 20x a second

    if (drew) {
      if (saverDrawn)
        drawAll(true);    // need to redraw if the saver had been drawn
    }

    if (!configuration.screen.saverDisable)
      saverDrawn = updateSaver(drew);
  } else {
    if (!configuration.screen.saverDisable)
      saverDrawn = updateSaver(false);
  }
}
