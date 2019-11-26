#ifndef _INCLUDE_STATE_H_
#define _INCLUDE_STATE_H_

#include <stdint.h>

struct Settings {
  uint16_t    numberMeasures;
  uint16_t    beatsPerMeasure;
  uint16_t    beatUnit;

  uint16_t    tupletCount;
  uint16_t    tupletTime;
  uint16_t    tupletUnit;
};

extern double bpm;
extern Settings settings;


#endif // _INCLUDE_STATE_H_
