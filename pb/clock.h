#ifndef _INCLUDE_CLOCK_H_
#define _INCLUDE_CLOCK_H_

#include "state.h"
#include "timing.h"



enum ClockState : uint16_t {
  clockPaused       = 0,
    // outputs are off;
    // ext. clocks have stopped long enough to consider this a pause
    // Note: likely to have stopped a beat ahead of the source, when it
    // restarts, there will be a period of re-syncing the alignment.

  clockPerplexed    = 1,
    // Just like sync, but the clock rate doesn't make sense and so position
    // cannot be assured

  clockSyncRunning  = 2,
    // Normal running mode, estimating tempo from external clocks
    // position is tracked, and phase locking implemented

  clockFreeRunning  = 3,
    // Running with a fixed tempo, extneral clocks are ignored.
};


struct ClockStatus {
public:
  static ClockStatus current();

  bpm_t       bpm;
  ClockState  state;
  bool        stopped;

  inline bool advancing() const
    { return !stopped && state != clockPaused; }

  inline ClockStatus(bpm_t b, ClockState s, bool t) : bpm(b), state(s), stopped(t) { }
  inline ClockStatus() : bpm(0), state(clockPaused), stopped(false) { }
 };

inline bool operator==(const ClockStatus& rhs, const ClockStatus& lhs) {
  return rhs.bpm == lhs.bpm
    && rhs.state == lhs.state
    && rhs.stopped == lhs.stopped;
}
inline bool operator!=(const ClockStatus& rhs, const ClockStatus& lhs) {
  return !(rhs == lhs);
}


void initializeClock();

void setBpm(bpm_t);
  // if sync'd, this resets the current BPM, but system will immediately try to
  // sync back to externally driven tempo.
void setSync(SyncMode);
void setOtherSync(OtherSyncMode);
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
