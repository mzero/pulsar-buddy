#ifndef _INCLUDE_TIMER_HW_H_
#define _INCLUDE_TIMER_HW_H_

#include "state.h"
#include "timing.h"

void initializeTimers(const State&, void (*onMeasure)());



bpm_t currentBpm();
void setBpm(bpm_t);
  // if sync'd, this resets the current BPM, but system will immediately try to
  // sync back to externally driven tempo.
void setSync(SyncMode);


void resetTimers(const Settings&);
void updateTimers(const Settings&);

void dumpTimer();
void dumpCapture();

#endif // _INCLUDE_TIMER_HW_H_
