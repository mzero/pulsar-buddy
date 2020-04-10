#ifndef _INCLUDE_PINS_H_
#define _INCLUDE_PINS_H_

#include <variant.h>


class TriggerOutput {
public:
  void initialize();
  void forceOff(bool);

  void testInitialize();
  void testWrite(bool);


  static TriggerOutput S;
  static TriggerOutput M;
  static TriggerOutput B;
  static TriggerOutput T;

private:
  TriggerOutput(uint32_t pin) : pin(pin) { }

  const uint32_t pin;
};


class TriggerInput {
public:
  void initialize();

  void testInitialize();
  bool testRead();

  static TriggerInput C;
  static TriggerInput O;

private:
  TriggerInput(uint32_t pin) : pin(pin) { }

  const uint32_t pin;
};


#endif // _INCLUDE_PINS_H_
