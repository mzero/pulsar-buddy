#include "clock.h"

#include <Arduino.h>

#include "config.h"
#include "timer_hw.h"

#define DEBUG_CAPTURE 0
#define DEBUG_ISR 1

// In this section of code, be very careful about numeric types
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wconversion"

namespace {

  Timing activeTiming;

  // the divisor currently set on the timer, may jitter to correct phase
  divisor_t activeDivisor = 0;  // so it will be set the first time

  // the divisor that matches the sync'd BPM
  divisor_t targetDivisor = 0;

  void setDivisors(divisor_t target, divisor_t active) {
    if (activeDivisor != active) {
      // FIXME: would be better to adjust count to match remaining fration used
      // would be more efficient to keep previous divisor so no need to sync
      // read it back from the timer again
      setQuantumDivisor(active);
      activeDivisor = active;
    }

    if (target != 0 && targetDivisor != target) {
      updateWidths(target, activeTiming);
      targetDivisor = target;
    }
  }

  /** CLOCK STATE **/

   enum ClockMode {
    modeFirsttime,
    modeInternal,
    modeExternal
  };

  ClockMode   clockMode = modeFirsttime;
  ClockState  clockState = clockPaused;

  void setState(ClockState cs) {
    if (clockState == cs)
      return;

    bool wasRunning = runningState(clockState);
    bool setRunning = runningState(cs);

    clockState = cs;

    if (wasRunning == setRunning)
      return;

    if (setRunning) {
      forceTriggersOff(false);
    }
    else {
      forceTriggersOff(true);
    }
  }

  void setMode(ClockMode cm) {
    if (clockMode == cm)
      return;
    clockMode = cm;

    switch (cm) {
      case modeInternal:
        setState(clockFreeRunning);
        break;

      case modeExternal:
        setState(clockPaused);
        break;

      default:
        break;
    }
  }

  void setCountsToPosition(q_t position) {
    position += activeTiming.sequence - 1;
    // Pre-roll the positions one clock earlier so that when the clock
    // starts, any output transitions for this position will fire.

    Offsets counts;
    setOffsets(activeTiming, position, counts);

    PauseQuantum pq;
    writeCounts(counts);
  }


  /** BPM RANGE **/

  divisor_t divisorMin = 952;   // 300 bpm
  divisor_t divisorMax = 9524;  // 30 bpm

  // When running, allow a 5% wider range so small excursions outside
  // the range don't make the clock perplexed. When perplexed, the normal
  // range is used, so that it only unperplexes when solidly good.
  divisor_t runningDivisorMin = 907;    // 315 bpm
  divisor_t runningDivisorMax = 10582;  // 27 bpm

  void setBpmRange(float low, float high) {
    divisorMin = divisorFromBpm(high);
    divisorMax = divisorFromBpm(low);

    runningDivisorMin = divisorFromBpm(high * 1.05f);
    runningDivisorMax = divisorFromBpm(low * 0.95f);
  }



  /** EXTERNAL CLOCK FREQ ESTIMATOR & PHASE LOCKED LOOP **/

  template< typename T >
  inline T roundingDivide(T x, T q) {
    return (x + q / 2) / q;
  }



  // These are used to communicate changes in the clocking to the interrupt
  // service routine.
  volatile int externalClocksPerBeat = 0;
  volatile bool pendingExternalClocksPerBeatChange = false;

  volatile q_t nextPosition = 0;
  volatile bool pendingNextPosition = false;

  // These variables "belong" to the interrupt service routines, isrClock()
  // and isrWatchdog().
  // They should not be modified, or even read, outside of those.
  int       capturesPerBeat = 0;
  q_t       captureClkQ = 0;
  q_t       captureClkQWait = 0;
  q_t       captureSequencePeriod = 0;
  const int captureBufferBeats = 2;
  const int captureBufferSize = captureBufferBeats * 48;
  divisor_t captureBuffer[captureBufferSize];
  int       captureBufferSpan;
  int       captureNext = 0;
  uint32_t  captureSum = 0;
  int       captureCount = 0;

  bool captureLastSampleValid = false;
  q_t captureLastSample = 0;

  q_t currentPosition = 0;


  inline void processPending() {
    if (pendingExternalClocksPerBeatChange) {
      int perBeat = externalClocksPerBeat;
      if (capturesPerBeat != perBeat) {
        capturesPerBeat = perBeat;
        captureClkQ = perBeat ? Q_PER_B / perBeat : 0;
        captureClkQWait = max(Q_PER_B / 4, captureClkQ * 2);
        captureBufferSpan = captureBufferBeats * perBeat;
        captureNext = 0;
        captureSum = 0;
        captureCount = 0;
        captureLastSampleValid = false;
      }
      pendingExternalClocksPerBeatChange = false;
    }
  }

  inline void setClockRate(int perBeat) {
    // noInterrupts();  // shouldn't be needed given the access pattern
    externalClocksPerBeat = perBeat;
    pendingExternalClocksPerBeatChange = true;
    // interrupts();
  }

  inline void zeroCapture() {
    captureNext = 0;
    captureSum = 0;
    captureCount = 0;
  }
}

#include "clock_debug.hpp"


void isrWatchdog() {
  if (clockMode == modeInternal)
    return;

  DebugWatchdogIsr debug;

  captureLastSampleValid = false;

  switch (clockState) {
    case clockStopped:
      // ignore - no external clock while explicitly stopped is okay
      break;

    case clockPaused:
      // already paused.... and 2nd watchdog!
      zeroCapture();
      break;

    default:
      // pause, as an external clock hasn't been heard in too long
      setState(clockPaused);
      resetWatchdog(4 * Q_PER_B);

      // When the next external clock eventually shows up, it corresponds to
      // one clock after the last position - so set that position pending.
      nextPosition = currentPosition + captureClkQ;
      pendingNextPosition = true;
  }
}

void isrClockCapture(q_t sequenceSample) {
  processPending();

  if (captureSequencePeriod != activeTiming.sequence) {
    captureSequencePeriod = activeTiming.sequence;
    captureLastSampleValid = false;
      // can't rely on last sanple if period changd
    if (currentPosition >= captureSequencePeriod) {
      currentPosition = currentPosition % captureSequencePeriod;
      setCountsToPosition(currentPosition);
      sequenceSample = currentPosition;
    }
  }

  if (pendingNextPosition) {
    currentPosition = nextPosition % captureSequencePeriod;
    pendingNextPosition = false;
    setCountsToPosition(currentPosition);
    sequenceSample = currentPosition;
  } else {
    if (runningState(clockState))
      currentPosition = (currentPosition + captureClkQ) % captureSequencePeriod;
  }

  if (clockMode == modeInternal)
    return;

  DebugClockIsr debug;

  bool dNextInRange = false;

  if (captureLastSampleValid) {

    // compute new divisor based on measurement of last ext. clock interval

    q_t qdiff =
      (sequenceSample + captureSequencePeriod - captureLastSample)
      % captureSequencePeriod;

    uint32_t dNext =
      roundingDivide((uint32_t)activeDivisor * qdiff, captureClkQ);
    debug.noteDNext(dNext);

    dNextInRange = 0 < dNext && dNext < 16000;   // FIXME: pickd out of a hat
    // if in a rational range, include in the running average

    uint32_t dFilt = 0;

    if (dNextInRange) {
      // record this in the capture buffer, and average it with up to
      // captureBufferBeats' worth of measurements
      captureSum += dNext;
      if (captureCount >= captureBufferSpan) {
        captureSum -= captureBuffer[captureNext];
      } else {
        captureCount += 1;
      }
      captureBuffer[captureNext] = (divisor_t)dNext;
      captureNext = (captureNext + 1) % captureBufferSpan;

      // compute the average ext clk rate over the last captureBufferBeats
      dFilt = roundingDivide(captureSum, (uint32_t)captureCount);
      debug.noteDFilt(dFilt);
    }

    uint32_t dAdj = dFilt;

    // find phase difference if we can
    if (clockState != clockStopped  &&  dFilt) {
      q_t ahead =
        (sequenceSample + captureSequencePeriod - currentPosition)
          % captureSequencePeriod;
      q_t behind = captureSequencePeriod - ahead;

      if (ahead == 0) {
        // do nothing!
      }
      else if (min(ahead, behind) > 2 * Q_PER_B) {  // FIXME: 4? 2? 1?
        // too far, just jump
        setCountsToPosition(currentPosition);
      }
      else if (behind < ahead) {
        // timers are behind... hurry up
        behind = constrain(behind, 0, Q_PER_B);
        dAdj = roundingDivide(dAdj * Q_PER_B, Q_PER_B + behind);
      }
      else {
        // timers are ahead... slow down
        ahead = constrain(ahead, 0, Q_PER_B / 2);
        dAdj = roundingDivide(dAdj * Q_PER_B, Q_PER_B - ahead);
      }

      if (dAdj != dFilt) {
        debug.noteDAdj(dAdj);
      }
    }

    dNext = constrain(dAdj ? dAdj : dNext, runningDivisorMin, runningDivisorMax);
    setDivisors((divisor_t)dFilt, (divisor_t)dNext);
  }

  captureLastSample = sequenceSample;
  captureLastSampleValid = true;

  bool slowWatchDog = true;

  switch (clockState) {
    case clockPaused:
      setState(clockSyncRunning);
      break;
    case clockPerplexed:
      if (dNextInRange)
        setState(clockSyncRunning);
      break;
    case clockSyncRunning:
      if (dNextInRange)
        slowWatchDog = false;
      else
        setState(clockPerplexed);
    default:
      ;
  }

  resetWatchdog(slowWatchDog ? 4 * Q_PER_B : captureClkQWait);
}



ClockStatus ClockStatus::current() {
  static auto updateAt = millis() - 1; // ensure update the first time
  static float reportedBpmf = 0;
  static bpm_t reportedBpm = 0;
  static const float fastThreasholdDeltaBpmf = 0.5f;
    // If moving faster than this, then just jump

  auto now = millis();
  if (updateAt < now) {
    updateAt = max(updateAt + 100, now + 10);
      // recompute only 10x a second, on a regular basis, but don't get behind

    auto targetBpmf = divisorToBpm(targetDivisor);
    if (fabsf(targetBpmf - reportedBpmf) < fastThreasholdDeltaBpmf) {
      // moving a little bit, so just slew slowly
      reportedBpmf = (10.0f * reportedBpmf + targetBpmf) / 11.0f;
    } else {
      // moved a lot
      reportedBpmf = targetBpmf;
    }

    reportedBpm = (bpm_t)(roundf(reportedBpmf));

    if (configuration.debug.plotClock) {
      // for plotting on the IDE plotter
      int targetBpm100 = (int)(roundf(100.0f * targetBpmf));
      int reportedBpm100 = (int)(roundf(100.0f * reportedBpmf));

      Serial.printf(
        "targetf:%d.%02d reportedf:%d.%02d reported:%d\n",
        targetBpm100 / 100, targetBpm100 % 100,
        reportedBpm100 / 100, reportedBpm100 % 100,
        reportedBpm
        );
    }
  }

  return ClockStatus(reportedBpm, clockState);
}


void initializeClock() {
  if (configuration.options.extendedBpmRange)
    setBpmRange(10, 900);
  else
    setBpmRange(30, 300);

  startQuantumEvents();
}

void setBpm(bpm_t bpm) {
  bpm = constrain(bpm, bpmMin, bpmMax);
  divisor_t d = divisorFromBpm((float)bpm);
  setDivisors(d, d);
}

void setSync(SyncMode sync) {
  setClockRate(syncPpqn(sync));
  setMode(sync == syncInternal ? modeInternal : modeExternal);
  useExtClockSource(sync == syncMidiUSB ? extClockSoftware : extClockHardware);
}

void setTiming(const State& state) {
  computePeriods(state, activeTiming);
  {
    PauseQuantum pq;

    Offsets counts;
    readCounts(counts);
    adjustOffsets(activeTiming, counts);
    writePeriods(activeTiming, targetDivisor);
    writeCounts(counts);

    // FIXME: Needs to update position here
  }
}

void setPosition(q_t position) {
  switch (clockMode) {
    case modeExternal:
      nextPosition = position;
      pendingNextPosition = true;
      break;
    default:
      setCountsToPosition(position);
  }
}

void runClock() {
  setState(clockMode == modeInternal ? clockFreeRunning : clockPaused);
}

void stopClock() {
  setState(clockStopped);
}


#pragma GCC diagnostic pop



void dumpClock() {
#if DEBUG_CAPTURE
  if (capturesPerBeat == 0)
    return;

  uint32_t sum = 0;

  for (int i = 0; i < captureCount; ++i) {
    sum += captureBuffer[i];
    Serial.printf("[%2d] %8d\n", i, captureBuffer[i]);
  }

  if (captureCount > 0)
    Serial.printf("Sum: %8d, Average: %8d\n\n",
      sum, roundingDivide(sum, (uint32_t)captureCount));

  Serial.printf("captures per beat: %d | q/clock = %d\n", capturesPerBeat, captureClkQ);
  Serial.printf("capture count: %d\n", captureCount);
  Serial.printf("sequence period = %d\n", captureSequencePeriod);
  Serial.printf("captureBufferSpan = %d\n", captureBufferSpan);
  Serial.printf("captureNext = %d\n", captureNext);
  Serial.printf("captureSum = %d\n", captureSum);
  Serial.printf("captureCount = %d\n", captureCount);
#endif // DEBUG_CAPTURE

  int activeBpm100 = (int)(roundf(100.0f * divisorToBpm(activeDivisor)));
  int targetBpm100 = (int)(roundf(100.0f * divisorToBpm(targetDivisor)));

  Serial.printf("active: %3d.%02d bpm - %5d divisor  |  "
                "target: %3d.%02d bpm - %5d divisor\n",
    activeBpm100 / 100, activeBpm100 % 100, activeDivisor,
    targetBpm100 / 100, targetBpm100 % 100, targetDivisor);

  Serial.print("mode: ");
  switch (clockMode) {
    case modeFirsttime: Serial.print("first time"); break;
    case modeInternal:  Serial.print("internal");   break;
    case modeExternal:  Serial.print("external");   break;
    default:            Serial.print("???");
  }
  Serial.print(", state: ");
  switch (clockState) {
    case clockPaused:       Serial.println("paused");         break;
    case clockPerplexed:    Serial.println("perplexed");      break;
    case clockSyncRunning:  Serial.println("sync running");   break;
    case clockFreeRunning:  Serial.println("free running");   break;
    default:                Serial.println("???");
  }

  Offsets counts;
  readCounts(counts);
  Serial.println("position:");
  dumpOffsets(counts);

  DebugIsr::dumpHistory();
}

void clearClockHistory() { DebugIsr::clearHistory(); }
void dumpClockHistory() { DebugIsr::dumpHistory(); }
