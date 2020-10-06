#ifndef _INCLUDE_CLOCK_H_
#define _INCLUDE_CLOCK_H_

#include "state.h"
#include "timing.h"



enum ClockState : uint16_t {
  clockPaused       = 0,
  clockPerplexed    = 1,
  clockSyncRunning  = 2,
  clockFreeRunning  = 3,
};

inline bool runningState(ClockState cs)
  { return cs >= clockSyncRunning; }

struct ClockStatus {
public:
  static ClockStatus current();

  inline bool running() const
    { return state == clockFreeRunning || clockSyncRunning; }

  bpm_t       bpm;
  ClockState  state;

  inline ClockStatus(bpm_t b, ClockState s) : bpm(b), state(s) { }
  inline ClockStatus() : bpm(0), state(clockPaused) { }
 };

inline bool operator==(const ClockStatus& rhs, const ClockStatus& lhs) {
  return rhs.bpm == lhs.bpm && rhs.state == lhs.state;
}
inline bool operator!=(const ClockStatus& rhs, const ClockStatus& lhs) {
  return !(rhs == lhs);
}


void initializeClock();

void setBpm(bpm_t);
  // if sync'd, this resets the current BPM, but system will immediately try to
  // sync back to externally driven tempo.
void setSync(SyncMode);

// void midiClock();

void resetTiming(const State&);
void updateTiming(const State&);


// ISR routines

void isrClockCapture(q_t, q_t);
void isrWatchdog();


void dumpClock();
void clearClockHistory();
void dumpClockHistory();

#endif // _INCLUDE_CLOCK_H_
