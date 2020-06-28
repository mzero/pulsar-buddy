#ifndef _INCLUDE_TIMING_H_
#define _INCLUDE_TIMING_H_

#include <stdint.h>

#include "state.h"


/*
    This module works in terms of beats (B).
    In the UI, a beat is displayed as a quarter note.

    1/4 beat is the smallest "even" division, and any multiple of
    this can be then divided by [2..9] to make a tuplet.

    Hence, the smallest sub-beat unit, or quantum (Q) is:
      1/(4*LCM(1..9)) beat = 1/10,080 B
*/

typedef uint32_t q_t;

const q_t Q_PER_B = 10080;

template< typename T >
inline q_t qcast(const T& x) { return static_cast<q_t>(x); }


struct Timing {
  q_t   measure;
  q_t   sequence;
  q_t   beat;
  q_t   tuplet;
};

void computePeriods(const State& state, Timing& periods, Timing& widths);

void adjustOffsets(const Timing& periods, Timing& offsets);



typedef uint16_t bpm_t;

const bpm_t bpmMin = 30;
const bpm_t bpmMax = 300;


void dumpQ(q_t);
void dumpTiming(const Timing&);


#endif // _INCLUDE_TIMING_H_
