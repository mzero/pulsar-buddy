// #include <ZeroRegs.h>

#if 0
#include <usbmidi.h>
#endif

#include "display.h"
#include "layout.h"
#include "state.h"
#include "timer_hw.h"
#include "ui_input.h"


// OLED FeatherWing buttons map to different pins depending on board:
#if defined(ESP8266)
  #define BUTTON_A  0
  #define BUTTON_B 16
  #define BUTTON_C  2
#elif defined(ESP32)
  #define BUTTON_A 15
  #define BUTTON_B 32
  #define BUTTON_C 14
#elif defined(ARDUINO_STM32_FEATHER)
  #define BUTTON_A PA15
  #define BUTTON_B PC7
  #define BUTTON_C PC5
#elif defined(TEENSYDUINO)
  #define BUTTON_A  4
  #define BUTTON_B  3
  #define BUTTON_C  8
#elif defined(ARDUINO_FEATHER52832)
  #define BUTTON_A 31
  #define BUTTON_B 30
  #define BUTTON_C 27
#else // 32u4, M0, M4, nrf52840 and 328p
  #define BUTTON_A  9
  #define BUTTON_B  6
  #define BUTTON_C  5
#endif

#define ENCODER_SW  10
#define ENCODER_A   11
#define ENCODER_B   12


IdleTimeout selectionTimeout(2000);
IdleTimeout dimTimeout(5000);
Encoder encoder(ENCODER_A, ENCODER_B);
Button encoderButton(ENCODER_SW);
Button oledButtonA(BUTTON_A);
Button oledButtonB(BUTTON_B);
Button oledButtonC(BUTTON_C);

// ZeroRegOptions zeroOpts = { Serial, true };

bool measureEvent = false;

void noteMeasure() {
  measureEvent = true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial);
     // wait for native usb, there is a delay in that call
     // so no need for one here
     // FIXME: Take this out for production

  initializeState();
  initializeTimers(userState(), noteMeasure);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32
  drawAll(true);
}


void loop() {
  bool active = false;

  if (measureEvent) {
    measureEvent = false;
    if (pendingState()) {
      resetTimers(userSettings());
      commitState();
      active = true;
    }
  } else {
    auto bpm = currentBpm();
    if (userState().userBpm != bpm) {
      userState().userBpm = bpm;
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

  int dir = encoder.update();
  if (dir) {
    updateSelection(dir);
    active = true;
  }

  ButtonState s = encoderButton.update();
  if (s != buttonNoChange) {
    clickSelection(s);
    active = true;
  }

  if (encoderButton.active()) {
    active = true;
  }

  if (oledButtonA.update() == buttonDown) {
    resetTimers(userSettings());
    active = true;
  }

  if (oledButtonB.update() == buttonDown) {
    //printZeroRegs(zeroOpts);
    active = true;
  }

  if (oledButtonC.update() == buttonDown) {
    // storeToMemory(1);
    dumpCapture();
    // dumpTimer();
    active = true;
  }

  if (active) {
    selectionTimeout.activity();
    dimTimeout.activity();
    drawAll(false);
  } else {
    if (selectionTimeout.update()) {
      resetSelection();
      drawAll(false);
    }
    if (dimTimeout.update()) {
      display.dim(true);
    }
  }
}
