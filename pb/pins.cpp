#include "pins.h"

#include <Arduino.h>
#include <wiring_private.h>

/*
  Output pins are:
                timer   wave    samd pin    arduino pin
    SEQUENCE:   TCC0    WO4     PB10        29, PIN_SPI_MOSI
    MEASURE:    TCC1    WO0     PA10         1, PIN_SERIAL1_TX
    BEAT:       TC5     WO1     PB11        30, PIN_SPI_SCK
    TUPLET:     TCC2    WO0     PA12        28, PIN_SPI_MISO

    Despite the Adafruit's pinout diagram, these are the actual library pin
    numbers for the SPI pins on the Feather M0 Express.

  Input pins:
    EXT CLK:                    PB08        15, PIN_A1
    OTHER:                      PB09        16, PIN_A2


  NB: If you want to change the pin assignments, then you must carefully
  review and adjust the timer assignments in timer_hw.cpp so that each
  timer has an output waveform that can be assigned to the pin you want to
  use.

*/

TriggerOutput TriggerOutput::S(PIN_SPI_MOSI);
TriggerOutput TriggerOutput::M(PIN_SERIAL1_TX);
TriggerOutput TriggerOutput::B(PIN_SPI_SCK);
TriggerOutput TriggerOutput::T(PIN_SPI_MISO);

TriggerInput TriggerInput::C(PIN_A1);
TriggerInput TriggerInput::O(PIN_A2);


// NORMAL OPERATION

void TriggerOutput::initialize() {
  // Pins are configured as outputs, first, set to HIGH...
  // ...then configured to output the from the peripheral multiplexor.
  // This way, all triggers can be forced off (HIGH, they invert), by just
  // disabling the multiplexor for them.

  digitalWrite(pin, HIGH);
  pinPeripheral(pin, pin == PIN_SPI_MOSI ? PIO_TIMER_ALT : PIO_TIMER);
}

void TriggerOutput::disable(bool off) {
  PORT->Group[g_APinDescription[pin].ulPort]
    .PINCFG[g_APinDescription[pin].ulPin]
      .bit.PMUXEN = off ? 0 : 1;
}

void TriggerInput::initialize() {
  pinMode(pin, INPUT_PULLUP);
  pinPeripheral(pin, PIO_EXTINT);
}


// TEST MODE OPERATION

void TriggerOutput::testInitialize() {
  pinMode(pin, OUTPUT);
  pinPeripheral(pin, PIO_OUTPUT);
}

void TriggerOutput::testWrite(bool on) {
  digitalWrite(pin, on ? LOW : HIGH);   // outputs are inverted
}


void TriggerInput::testInitialize() {
  pinMode(pin, INPUT_PULLUP);
  pinPeripheral(pin, PIO_INPUT_PULLUP);
}

bool TriggerInput::testRead() {
  return digitalRead(pin) == LOW;     // inputs are inverted
}

