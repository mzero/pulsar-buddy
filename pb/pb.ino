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

void setup() {
  Serial.begin(9600);
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32
  drawAll(true);

  initializeTimers(bpm);
}

void postAction() {
  drawAll(false);
  selectionTimeout.activity();
  dimTimeout.activity();
  setTimerBpm(bpm);
}

void loop() {
  // delay(10);
  // yield();

  int dir = encoder.update();
  if (dir) {
    updateSelection(dir);
    postAction();
    return;
  }

  if (encoderButton.update()) {
    clickSelection();
    postAction();
    return;
  }

  if (oledButtonA.update()) {
    bpm += 10.0;
    postAction();
    return;
  }

  if (oledButtonB.update()) {
    bpm -= 10.0;
    postAction();
    return;
  }

  if (oledButtonC.update()) {
    dumpTimer();
    return;
  }

  if (selectionTimeout.update()) {
    resetSelection();
    drawAll(false);
    return;
  }

  if (dimTimeout.update()) {
    display.dim(true);
    return;
  }
}
