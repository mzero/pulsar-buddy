// #include <ZeroRegs.h>

#include "clock.h"
#include "config.h"
#include "controls.h"
#include "critical.h"
#include "display.h"
#include "layout.h"
#include "midi.h"
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
  initializeDisplay();

  Configuration::initialize();

  initializeMidi();

  if (configuration.debug.waitForSerial) {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.print("Waiting for\nserial connection...");
    display.display();

    while (!Serial);
       // wait for native usb, there is a delay in that call
       // so no need for one here
  }

  initializeState();
  initializeTimers();
  initializeClock();

  setBpm(userState().userBpm);
  setSync(userState().syncMode);
  resetTiming(userState());

  if (configuration.options.alwaysDim)
    display.dim(true);

  if (critical.update() != Critical::active)
    drawAll(true);

  updateSaver(true);
  selectionTimeout.activity();
  dimTimeout.activity();

  auto s1 = sramUsed();
  Serial.printf("sram used: %d static, %d post-init\n", s0, s1);
}


void loop() {
  bool active = false;

  updateMidi();

  if (measureEvent) {
    measureEvent = false;
    if (pendingState()) {
      updateTiming(userState());
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

  switch (critical.update()) {
    case Critical::inactive: break;
    case Critical::active: return;
    case Critical::closed: drawAll(true); break;
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
    dumpDisplayPBM(Serial);
    //resetTiming(userState());
    active = true;
  }

  if (oledButtonB.update() == Button::Down) {
#ifdef ZERO_REGS_H
    // printZeroRegs(zeroOpts);
    printZeroRegTCC(zeroOpts, TCC0, 0);
#endif
    //critical.printf("ouch @ %dms\n", millis());
    //critical.println("  something went wrong");
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
    if (!configuration.options.alwaysDim)
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

    if (!configuration.options.saverDisable)
      saverDrawn = updateSaver(drew);
  } else {
    if (!configuration.options.saverDisable)
      saverDrawn = updateSaver(false);
  }
}
