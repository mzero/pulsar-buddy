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
  lastRead = -1;        // will cause first update to always set it
  validAtTime = 0;

  state = buttonUp;
  longAtTime = 0;
}

ButtonState Button::update()
{
  int read = digitalRead(pin);
  if (read != lastRead) {
    // pin changed, wait for it to be stable
    lastRead = read;
    validAtTime = millis() + 50;
    return buttonNoChange;
  }

  uint32_t now = millis();
  if (now < validAtTime) {
    // pin stable, not not long enough
    return buttonNoChange;
  }

  ButtonState prevState = state;

  switch (state) {
    case buttonUp:
    case buttonUpLong:
      if (lastRead == LOW) {
        state = buttonDown;
        longAtTime = now + 2000;
      }
      break;

    case buttonDown:
      if (lastRead == LOW) {  // still down?
        if (now > longAtTime) {
          state = buttonDownLong;
          break;
        }
      }
      // fall through

    case buttonDownLong:
      if (lastRead == HIGH) {
        state = (prevState == buttonDownLong) ? buttonUpLong : buttonUp;
      }
      break;

    default:
      break;
  }

  return (state != prevState) ? state : buttonNoChange;
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

