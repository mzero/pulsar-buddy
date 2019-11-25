#include "ui_input.h"

#include "delay.h"
#include "wiring_constants.h"
#include "wiring_digital.h"



Encoder::Encoder(uint32_t pinA, uint32_t pinB)
    : pinA(pinA), pinB(pinB)
{
  pinMode(pinA, INPUT_PULLUP);
  pinMode(pinB, INPUT_PULLUP);
  a = digitalRead(pinA);
  b = digitalRead(pinB);
  quads = 0;
}

int Encoder::update() {
  int newA = digitalRead(pinA);
  int newB = digitalRead(pinB);

  int r = 0;

  if (newA != a || newB != b) {
    if (newA == a) {
      quads += (newA == newB) ? 1 : -1;
    } else if (newB == b) {
      quads += (newA != newB) ? 1 : -1;
    }

    a = newA;
    b = newB;

    if (a && b) {
      if (quads > 1) {
        r = 1;
      } else if (quads < -1) {
        r = -1;
      }
      quads = 0;
    }
  }

  return r;
}


Button::Button(uint32_t pin)
  : pin(pin)
{
  pinMode(pin, INPUT_PULLUP);   // 1 is off, 0 is pressed
  lastState = 1;
  lastRead = -1;        // will cause first update to always set it
  validAtTime = 0;
}

bool Button::update()
{
  int read = digitalRead(pin);
  if (read != lastRead) {
    lastRead = read;
    validAtTime = millis() + 50;
    return false;
  }
  if (lastRead == lastState) {
    return false;
  }
  uint32_t now = millis();
  if (now >= validAtTime) {
    lastState = lastRead;
    return lastState == 0;
  }
  return false;
}


IdleTimeout::IdleTimeout(unsigned long period)
  : idle(true), period(period)
  { }

void IdleTimeout::activity() {
  idle = false;
  idleAtTime = millis() + period;
}

bool IdleTimeout::update() {
  if (idle)
    return false;

  if (millis() > idleAtTime) {
    idle = true;
    return true;
  }

  return false;
}

