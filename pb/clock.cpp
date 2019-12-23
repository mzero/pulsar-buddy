#include "clock.h"

#include <Arduino.h>

#include "timer_hw.h"


// In this section of code, be very careful about numeric types
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wconversion"

namespace {
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

    targetDivisor = target;
  }


  Timing activePeriods;


  /** CLOCK STATE **/

   enum ClockMode {
    modeFirsttime,
    modeInternal,
    modeExtenral
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
      Timing preZeros;
      preZeros.measure = activePeriods.measure - 1;
      preZeros.sequence = activePeriods.sequence - 1;
      preZeros.beat = activePeriods.beat - 1;
      preZeros.tuplet = activePeriods.tuplet - 1;
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

      case modeExtenral:
        setState(clockPaused);
        break;

      default:
        break;
    }
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
}


void isrWatchdog() {
  if (clockMode == modeInternal)
    return;
  if (clockState == clockPaused)
    return;

  // pause, as an external clock hasn't been heard in too long
  setState(clockPaused);
  zeroCapture();
  captureLastSampleValid = false;
}

void isrClockCapture(q_t sequenceSample, q_t watchdogSample) {
  processPending();
  if (clockMode == modeInternal)
    return;

  if (captureSequencePeriod != activePeriods.sequence) {
    captureSequencePeriod = activePeriods.sequence;
    captureLastSampleValid = false;
      // can't rely on last sanple if period changd
  }

  q_t sample =
    clockState == clockPaused ? 0    // treat this clock as start of the sequence
    : clockState == clockPerplexed ? watchdogSample
    : sequenceSample;
  q_t period =
    clockState == clockPerplexed ? 0x10000
    : captureSequencePeriod;

  if (captureLastSampleValid) {

    // compute current ext clk rate, as a divisor
    q_t qdiff = (sample + period - captureLastSample) % period;

    uint32_t dNext =
      roundingDivide((uint32_t)activeDivisor * qdiff, captureClkQ);

    if (divisorMin <= dNext && dNext <= divisorMax) {
      if (clockState != clockSyncRunning)
        zeroCapture();

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
      q_t phase = sample % captureClkQ;
      if (phase >= captureClkQHalf)
        phase -= captureClkQ;

      // adjust filterd divisor to fix the phase error over one beat
      uint32_t dAdj = roundingDivide(dFilt * Q_PER_B, Q_PER_B - phase);
      dAdj = constrain(dAdj, divisorMin, divisorMax);

      setDivisors((divisor_t)dFilt, (divisor_t)dAdj);
      setState(clockSyncRunning);
      resetWatchdog(captureClkQWait);
    } else {
      setState(clockPerplexed);
      resetWatchdog(4 * Q_PER_B);
        // on perplexed, wait much long for absence of clock to signal
        // paused
    }

  } else {
    if (clockState == clockPaused) {
      zeroCapture();
      setState(clockSyncRunning);
      resetWatchdog(4 * Q_PER_B);
        // on restart, the rate isn't established
        // so don't interpret slower than expected as stopped
    }
  }

  captureLastSampleValid = true;
  captureLastSample = sample;
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


#if 0 // for plotting on the IDE plotter
    int targetBpm100 = (int)(roundf(100.0f * targetBpmf));
    int reportedBpm100 = (int)(roundf(100.0f * reportedBpmf));

    Serial.printf(
      "targetf:%d.%02d reportedf:%d.%02d reported:%d\n",
      targetBpm100 / 100, targetBpm100 % 100,
      reportedBpm100 / 100, reportedBpm100 % 100,
      reportedBpm
      );
#endif
  }

  return ClockStatus(reportedBpm, clockState);
}

void setBpm(bpm_t bpm) {
  bpm = constrain(bpm, bpmMin, bpmMax);
  divisor_t d = divisorFromBpm((float)bpm);
  setDivisors(d, d);
}

void setSync(SyncMode sync) {
  int clocksPerBeat = 0;
  switch (sync) {
    case syncFixed: clocksPerBeat = 0; break;
    default:
      if (sync & syncExternalFlag) {
        clocksPerBeat = sync & syncPPQNMask;
      } else {
        clocksPerBeat = 0;
      }
  }

  setClockRate(clocksPerBeat);
  setMode(sync == syncFixed ? modeInternal : modeExtenral);
}

#pragma GCC diagnostic pop


void resetTiming(const Settings& settings) {
  computePeriods(settings, activePeriods);
  {
    PauseQuantum pq;
    zeroCounts();
    writePeriods(activePeriods);
  }
}

void updateTiming(const Settings& settings) {
  computePeriods(settings, activePeriods);
  {
    PauseQuantum pq;

    Timing counts;
    readCounts(counts);
    adjustOffsets(activePeriods, counts);
    writeCounts(counts);
    writePeriods(activePeriods);
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
    case modeExtenral:  Serial.print("external");   break;
  }
  Serial.print(", state: ");
  switch (clockState) {
    case clockPaused:       Serial.println("paused");         break;
    case clockPerplexed:    Serial.println("perplexed");      break;
    case clockSyncRunning:  Serial.println("sync running");   break;
    case clockFreeRunning:  Serial.println("free running");   break;
  }
}
