#ifndef _INCLUDE_TIMER_HW_H_
#define _INCLUDE_TIMER_HW_H_

#include "state.h"
#include "timing.h"

void initializeTimers(const State&, void (*onMeasure)());



struct ClockStatus {
public:
  static ClockStatus current();

  enum State : uint16_t {
    Running = 0,
    Paused = 1,
    Perplexed = 2
  };

  inline bool running() const { return state == Running; }

  bpm_t   bpm;
  State   state;

  inline ClockStatus(bpm_t b, State s) : bpm(b), state(s) { }
  inline ClockStatus() : bpm(0), state(Paused) { }
 };

inline bool operator==(const ClockStatus& rhs, const ClockStatus& lhs) {
  return rhs.bpm == lhs.bpm && rhs.state == lhs.state;
}
inline bool operator!=(const ClockStatus& rhs, const ClockStatus& lhs) {
  return !(rhs == lhs);
}


void setBpm(bpm_t);
  // if sync'd, this resets the current BPM, but system will immediately try to
  // sync back to externally driven tempo.
void setSync(SyncMode);

// void midiClock();

void resetTimers(const Settings&);
void updateTimers(const Settings&);

void dumpTimer();
void dumpCapture();

#endif // _INCLUDE_TIMER_HW_H_
