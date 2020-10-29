#ifndef _INCLUDE_CONFIG_H_
#define _INCLUDE_CONFIG_H_

#include <stdint.h>


struct Configuration {
  struct {
    uint8_t alwaysDim:1;
    uint8_t saverDisable:1;
    uint8_t extendedBpmRange:1;   // depreciated
    uint8_t flipDisplay:1;
    uint8_t :4;
  } options;

  struct {
    uint8_t waitForSerial:1;
    uint8_t flash:1;
    uint8_t timing:1;
    uint8_t plotClock:1;
    uint8_t :4;
  } debug;

  uint8_t reserved1;
  uint8_t reserved2;

  static void initialize();
};

extern Configuration configuration;


#endif // _INCLUDE_CONFIG_H_
