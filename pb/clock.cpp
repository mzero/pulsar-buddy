#include "clock.h"

#include <Arduino.h>

#include "config.h"
#include "timer_hw.h"

#define DEBUG_CAPTURE 0
#define DEBUG_ISR 0

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


  bool        clockInternal = false;
  ClockState  clockState = clockPaused;
  bool        clockStopped = false;


  inline bool advancing() { return !clockStopped && clockState > clockPaused; }

  void setState(ClockState cs) {
    clockState = cs;
    disableTriggers(!advancing());
  }

  void setStopped(bool s) {
    clockStopped = s;
    disableTriggers(!advancing());
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

  /** OtherSync **/

  OtherSyncMode otherSyncMode = otherSyncNone;
  q_t           otherSyncAlignment = 0;

  void recomputeAlignment() {
    q_t base = 0;

    switch (otherSyncMode) {
      case otherSyncAlignBeat:     base = 1 * Q_PER_B;             break;
      case otherSyncAlign2Beat:    base = 2 * Q_PER_B;             break;
      case otherSyncAlign4Beat:    base = 4 * Q_PER_B;             break;
      case otherSyncAlignMeasure:  base = activeTiming.periodM;    break;
      case otherSyncAlignSequence: base = activeTiming.periodS;    break;
      default:
        otherSyncAlignment = 0;
        return;
    }

    // quick GCD calculation here
    q_t a = activeTiming.periodS;
    q_t b = base;
    while (b > 0) {
      q_t r = a % b;
      a = b;
      b = r;
    }
    otherSyncAlignment = a;
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
        //captureClkQWait = max(Q_PER_B / 4, captureClkQ * 2);
        captureClkQWait = max(Q_PER_B / 4, captureClkQ * 4);
          // get watchdog out of the way for now
          // FIXME: figure out what the right multiplier is!
        captureBufferSpan = captureBufferBeats * perBeat;
        captureNext = 0;
        captureSum = 0;
        captureCount = 0;
        captureLastSampleValid = false;
      }
      pendingExternalClocksPerBeatChange = false;
    }
  }

  void alignPosition(q_t m) {
    if (m == 0) return;   // just to be safe

    q_t s = readSequenceCount();
    bool upcomingClk = 2*(s % captureClkQ) >= captureClkQ;

    q_t p = currentPosition;
    if (upcomingClk)      // if an external clock is about to happen
      p += captureClkQ;   // then lets align where that is going to be

    // find the place the sequence should be now
    q_t r = p % m;
    p -= r;               // align to multiple
    if (2*r > m)          // if position was closer to next multiple
      p += m;             // then place it there

    if (upcomingClk)      // if an external clock is about to happen
      p -= captureClkQ;   // then say we are just before it

    currentPosition = (p + captureSequencePeriod) % captureSequencePeriod;
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
  if (clockInternal)
    return;

  DebugWatchdogIsr debug;

  captureLastSampleValid = false;

  switch (clockState) {
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

  // how far did the sequence advance between ext. clocks
  q_t qdiff = captureClkQ;
  if (captureLastSampleValid)
    qdiff = (sequenceSample + captureSequencePeriod - captureLastSample)
      % captureSequencePeriod;

  {
    bool forcePosition = false;

    if (!clockStopped)
      currentPosition = (currentPosition + captureClkQ) % captureSequencePeriod;

    if (captureSequencePeriod != activeTiming.sequence) {
      // switch to new sequence length as requested
      captureSequencePeriod = activeTiming.sequence;

      // if current position is "too far".. will need to correct it
      if (currentPosition >= captureSequencePeriod) {
        currentPosition = currentPosition % captureSequencePeriod;
        forcePosition = true;
      }
    }

    if (pendingNextPosition) {
      pendingNextPosition = false;
      currentPosition = nextPosition % captureSequencePeriod;
      forcePosition = true;
    }

    if (forcePosition) {
      setCountsToPosition(currentPosition);
      sequenceSample = currentPosition;
    }
  }

  if (clockInternal)
    return;

  DebugClockIsr debug;

  bool dNextInRange = false;

  if (captureLastSampleValid) {
    debug.noteQDiff(qdiff);

    // compute new divisor based on measurement of last ext. clock interval
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
    if (!clockStopped  &&  dFilt) {
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
        sequenceSample = currentPosition;
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
      if (!captureLastSampleValid || dNextInRange)
        slowWatchDog = false;
      else
        setState(clockPerplexed);
    default:
      ;
  }

  resetWatchdog(slowWatchDog ? 4 * Q_PER_B : captureClkQWait);


  captureLastSample = sequenceSample;
  captureLastSampleValid = true;
}


void isrOtherSync(bool otherState) {
  switch (otherSyncMode) {
    case otherSyncNone:
      break;

    case otherSyncRunStop:
      if (otherState) setPosition(0);
      // fall through
    case otherSyncRunPause:
      setStopped(!otherState);
      break;

    case otherSyncRunStopToggle:
      if (otherState)
        if (clockStopped) setPosition(0);
      // fall through
    case otherSyncRunPauseToggle:
      if (otherState)
        setStopped(!clockStopped);
      break;

    case otherSyncAlignBeat:
    case otherSyncAlign2Beat:
    case otherSyncAlign4Beat:
    case otherSyncAlignMeasure:
    case otherSyncAlignSequence:
      // alignment has already been computed (as GCD of mode and sequence)
      alignPosition(otherSyncAlignment);
      break;
  }
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

  return ClockStatus(reportedBpm, clockState, clockStopped);
}


void initializeClock() {
  // TODO: Once extendedBpmRange config is deprecated, simplify this code.
  // For now, just force it extended.
  if (true || configuration.options.extendedBpmRange)
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
  clockInternal = sync == syncInternal;
  useExtClockSource(sync == syncMidiUSB ? extClockSoftware : extClockHardware);

  switch (sync) {
    case syncInternal:  setState(clockFreeRunning); break;
    default:            setState(clockPaused);      break;
  }

  setStopped(false);  // reset stopped state when changing sync
}

void setOtherSync(OtherSyncMode o) {
  otherSyncMode = o;
  recomputeAlignment();
}

void setTiming(const State& state) {
  computePeriods(state, activeTiming);
  {
    PauseQuantum pq;

    if (clockInternal) {
      Offsets counts;
      readCounts(counts);
      adjustOffsets(activeTiming, counts);
      writeCounts(counts);
    }
    writePeriods(activeTiming, targetDivisor);
    recomputeAlignment();
  }
}

void setPosition(q_t position) {
  if (clockInternal) {
    setCountsToPosition(position);
  } else {
    nextPosition = position;
    pendingNextPosition = true;
  }
}

void runClock() {
  setStopped(false);
}

void stopClock() {
  setStopped(true);
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

  Serial.printf("source: %s", clockInternal ? "internal" : "external");
  Serial.print(", state: ");
  switch (clockState) {
    case clockPaused:       Serial.print("paused");         break;
    case clockPerplexed:    Serial.print("perplexed");      break;
    case clockSyncRunning:  Serial.print("sync running");   break;
    case clockFreeRunning:  Serial.print("free running");   break;
    default:                Serial.print("???");
  }
  Serial.println(clockStopped ? ", stopped" : "");

  Offsets counts;
  readCounts(counts);
  Serial.println("position:");
  dumpOffsets(counts);

  DebugIsr::dumpHistory();
}

void clearClockHistory() { DebugIsr::clearHistory(); }
void dumpClockHistory() { DebugIsr::dumpHistory(); }
