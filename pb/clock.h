#ifndef _INCLUDE_CLOCK_H_
#define _INCLUDE_CLOCK_H_

#include "state.h"
#include "timing.h"



enum ClockState : uint16_t {
  clockStopped      = 0,
    // outputs are off;
    // external clocks may happen, but don't advance position
    // clock estimation may happen if external clocks do

  clockPaused       = 1,
    // outputs are off;
    // ext. clocks have stopped long enough to consider this a pause
    // Note: likely to have stopped a beat ahead of the source, when it
    // restarts, there will be a period of re-syncing the alignment.

  clockPerplexed    = 2,
    // Just like sync, but the clock rate doesn't make sense and so position
    // cannot be assured

  clockSyncRunning  = 3,
    // Normal running mode, estimating tempo from external clocks
    // position is tracked, and phase locking implemented

  clockFreeRunning  = 4,
    // Running with a fixed tempo, extneral clocks are ignored.
};


inline bool runningState(ClockState cs) {
  return cs >= clockPerplexed;
}



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
void setTiming(const State&);
void setPosition(q_t position);

void runClock();
void stopClock();

// ISR routines

void isrClockCapture(q_t);
void isrWatchdog();


// Debugging

void dumpClock();
void clearClockHistory();
void dumpClockHistory();

#endif // _INCLUDE_CLOCK_H_
