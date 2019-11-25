#ifndef _INCLUDE_UI_INPUT_H_
#define _INCLUDE_UI_INPUT_H_

#include <stdint.h>


class Encoder {
public:
  Encoder(uint32_t pinA, uint32_t pinB);
  int update();   // reports -1 for CCW, 0 for no motion, and 1 for CW

private:
  const uint32_t pinA;
  const uint32_t pinB;

  int a;
  int b;

  int quads;
};


class Button {
public:
    Button(uint32_t pin);
    bool update();    // reports true on press

private:
    const uint32_t pin;

    int lastState;
    int lastRead;
    unsigned long validAtTime;
};



class IdleTimeout {
public:
  IdleTimeout(unsigned long period);
  void activity();
  bool update();      // returns true on start of idle

private:
  const unsigned long period;
  bool idle;
  unsigned long idleAtTime;
};


#endif // _INCLUDE_UI_INPUT_H_
