#include "clock.h"

#include <Arduino.h>

#include "config.h"
#include "timer_hw.h"

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

    if (targetDivisor != target) {
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
      startQuantumEvents();
    }
    else {
      stopQuantumEvents();
      forceTriggersOff(true);

      // set just "before" zero so first quantum after pause will trigger
      // all the outputs
      Offsets preZeros;
      preZeros.countS = activeTiming.periodS - 1;
      preZeros.countM = activeTiming.periodM - 1;
      preZeros.countB = activeTiming.periodB - 1;
      preZeros.countT = activeTiming.periodT - 1;

      writeCounts(preZeros);
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

  // These variables "belong" to the interrupt service routines, capture()
  // and missedClock().
  // They should not be modified, or even read, outside of those.
  int       capturesPerBeat = 0;
  q_t       captureClkQ = 0;
  q_t       captureClkQHalf = 0;
  q_t       captureClkQWait = 0;
  q_t       captureSequencePeriod = 0;
  const int captureBufferBeats = 2;
  const int captureBufferSize = captureBufferBeats * 48;
  divisor_t captureBuffer[captureBufferSize];
  divisor_t captureHistory[captureBufferSize];
  int       captureBufferSpan;
  int       captureNext = 0;
  uint32_t  captureSum = 0;
  int       captureCount = 0;

  bool captureLastSampleValid = false;
  q_t captureLastSample = 0;

  q_t captureWatchdogStartCount = 0;

  inline void processPending() {
    if (pendingExternalClocksPerBeatChange) {
      int perBeat = externalClocksPerBeat;
      if (capturesPerBeat != perBeat) {
        capturesPerBeat = perBeat;
        captureClkQ = perBeat ? Q_PER_B / perBeat : 0;
        captureClkQHalf = captureClkQ / 2;
        captureClkQWait = captureClkQ * 2;  // TODO: 3? 2.5?
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

#if DEBUG_ISR
  uint32_t countWatchdogISR = 0;
  uint32_t countClockISR = 0;
  uint32_t countUnpausing = 0;
  uint32_t countStillPerplexed = 0;
  uint32_t countUnPerplexing = 0;
  uint32_t countRunningChangePeriod = 0;
  uint32_t countRunningInvalid = 0;
  uint32_t countRunningPerplexed = 0;
  uint32_t countRuningWell = 0;

  void zeroDebugCounts() {
    countWatchdogISR = 0;
    countClockISR = 0;
    countUnpausing = 0;
    countStillPerplexed = 0;
    countUnPerplexing = 0;
    countRunningChangePeriod = 0;
    countRunningInvalid = 0;
    countRunningPerplexed = 0;
    countRuningWell = 0;
  }

#define INC_COUNTER(c) (++c)
#define DEC_COUNTER(c) (--c)
#else
#define INC_COUNTER(c) (0)
#define DEC_COUNTER(c) (0)
#endif
}

void isrWatchdog() {
  if (clockMode == modeInternal)
    return;
  if (clockState == clockPaused)
    return;

  INC_COUNTER(countWatchdogISR);

  // pause, as an external clock hasn't been heard in too long
  setState(clockPaused);
  zeroCapture();
  captureLastSampleValid = false;
}

void isrClockCapture(q_t sequenceSample, q_t watchdogSample) {
  processPending();
  if (clockMode == modeInternal)
    return;

  INC_COUNTER(countClockISR);

  switch (clockState) {

    case clockPerplexed: {
      q_t qdiff = watchdogSample - captureWatchdogStartCount;

      uint32_t dNext =
        roundingDivide((uint32_t)activeDivisor * qdiff, captureClkQ);

      if (!(divisorMin <= dNext && dNext <= divisorMax)) {
          // still perplexed
          INC_COUNTER(countStillPerplexed);

          captureWatchdogStartCount = resetWatchdog(4 * Q_PER_B);
          break;
      }
      INC_COUNTER(countUnPerplexing);
      DEC_COUNTER(countUnpausing);
      // back to normal, act like un-pausing
      // fall through
    }

    case clockPaused: {
      INC_COUNTER(countUnpausing);

      zeroCapture();
      setState(clockSyncRunning);
      resetWatchdog(4 * Q_PER_B);
        // on restart, the rate isn't established
        // so don't interpret slower than expected as stopped

      captureLastSample = 0;
      captureLastSampleValid = true;
      break;
    }

    case clockSyncRunning: {
      if (captureSequencePeriod != activeTiming.sequence) {
        captureSequencePeriod = activeTiming.sequence;
        captureLastSampleValid = false;
          // can't rely on last sanple if period changd
        INC_COUNTER(countRunningChangePeriod);
      }

      INC_COUNTER(countRunningInvalid);
      if (captureLastSampleValid) {
        DEC_COUNTER(countRunningInvalid);

        q_t qdiff =
          (sequenceSample + captureSequencePeriod - captureLastSample)
          % captureSequencePeriod;

        uint32_t dNext =
          roundingDivide((uint32_t)activeDivisor * qdiff, captureClkQ);

        if (!(runningDivisorMin <= dNext && dNext <= runningDivisorMax)) {
          INC_COUNTER(countRunningPerplexed);
          setState(clockPerplexed);
          captureWatchdogStartCount = resetWatchdog(4 * Q_PER_B);
            // on perplexed, wait much long for absence of clock to signal paused
            // setting of the watchdog timer is the new last sample
          break;
        }

        // record this in the capture buffer, and average it with up to
        // captureBufferBeats' worth of measurements
        captureSum += dNext;
        if (captureCount >= captureBufferSpan) {
          captureSum -= captureBuffer[captureNext];
        } else {
          captureCount += 1;
        }
        captureBuffer[captureNext] = (divisor_t)dNext;

        // compute the average ext clk rate over the last captureBufferBeats
        uint32_t dFilt = roundingDivide(captureSum, (uint32_t)captureCount);
        captureHistory[captureNext] = (divisor_t)dFilt;

        captureNext = (captureNext + 1) % captureBufferSpan;

        // phase error (in Q)
        q_t phase = sequenceSample % captureClkQ;
        if (phase >= captureClkQHalf)
          phase -= captureClkQ;

        // adjust filterd divisor to fix the phase error over one beat
        uint32_t dAdj = roundingDivide(dFilt * Q_PER_B, Q_PER_B - phase);
        dAdj = constrain(dAdj, runningDivisorMin, runningDivisorMax);

        setDivisors((divisor_t)dFilt, (divisor_t)dAdj);
        setState(clockSyncRunning);
        resetWatchdog(captureClkQWait);
        INC_COUNTER(countRuningWell);
      }

      captureLastSample = sequenceSample;
      captureLastSampleValid = true;
    }

    case clockFreeRunning:
      break;
  }
}


ClockStatus ClockStatus::current() {
  static auto updateAt = millis() - 1; // ensure update the first time
  static float reportedBpmf = 0;
  static bpm_t reportedBpm = 0;
  static const float fastThreasholdDeltaBpmf = 1.0f;
    // If moving faster than this, then just jump
    // TODO: Consider setting this lower to 0.5f
    // That is good enough for the Digitakt, but too low for the the
    // janky clock divider test rig I have

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
}

void setBpm(bpm_t bpm) {
  bpm = constrain(bpm, bpmMin, bpmMax);
  divisor_t d = divisorFromBpm((float)bpm);
  setDivisors(d, d);
}

void setSync(SyncMode sync) {
  setClockRate(syncPpqn(sync));
  setMode(sync == syncInternal ? modeInternal : modeExternal);
}

#pragma GCC diagnostic pop


void resetTiming(const State& state) {
  computePeriods(state, activeTiming);
  {
    PauseQuantum pq;
    writePeriods(activeTiming, targetDivisor);
    zeroCounts();
  }
}

void updateTiming(const State& state) {
  computePeriods(state, activeTiming);
  {
    PauseQuantum pq;

    Offsets counts;
    readCounts(counts);
    adjustOffsets(activeTiming, counts);
    writePeriods(activeTiming, targetDivisor);
    writeCounts(counts);
  }
}

#if 0
void midiClock() {
  simulateExtClk();
}
#endif


void dumpClock() {
  if (capturesPerBeat == 0)
    return;

  uint32_t sum = 0;

  for (int i = 0; i < captureCount; ++i) {
    sum += captureBuffer[i];
    Serial.printf("[%2d] %8d -- %8d\n", i, captureBuffer[i], captureHistory[i]);
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
  }
  Serial.print(", state: ");
  switch (clockState) {
    case clockPaused:       Serial.println("paused");         break;
    case clockPerplexed:    Serial.println("perplexed");      break;
    case clockSyncRunning:  Serial.println("sync running");   break;
    case clockFreeRunning:  Serial.println("free running");   break;
  }

#if DEBUG_ISR
  Serial.printf("countWatchdogISR = %d\n", countWatchdogISR);
  Serial.printf("countClockISR = %d\n", countClockISR);
  Serial.printf("countUnpausing = %d\n", countUnpausing);
  Serial.printf("countStillPerplexed = %d\n", countStillPerplexed);
  Serial.printf("countUnPerplexing = %d\n", countUnPerplexing);
  Serial.printf("countRunningChangePeriod = %d\n", countRunningChangePeriod);
  Serial.printf("countRunningInvalid = %d\n", countRunningInvalid);
  Serial.printf("countRunningPerplexed = %d\n", countRunningPerplexed);
  Serial.printf("countRuningWell = %d\n", countRuningWell);

  zeroDebugCounts();
#endif
}
