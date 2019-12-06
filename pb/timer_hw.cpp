#include "timer_hw.h"

#include <initializer_list>

#include <Arduino.h>
#include <wiring_private.h>

#include "timing.h"


// *** NOTE: This implementation is for the SAMD21 only.
#if defined(__SAMD21__) || defined(TC4) || defined(TC5)

/* Timer Diagram:

  QUANTUM --> event --+--> BEAT --> waveform
   (TC4)              |    (TC5)
                      |
                      +--> SEQUENCE --> waveform
                      |    (TCC0)
                      |
                      +--> MEASURE --> waveform
                      |    (TCC1)
                      |
                      +--> TUPLET --> waveform
                           (TCC2)

*/

namespace {

  void defaultCallback() { }
  void (*tcc1callback)() = defaultCallback;

#define QUANTUM_EVENT_CHANNEL   5
#define EXTCLK_EVENT_CHANNEL    6

  void startQuantumEvents() {
    EVSYS->CHANNEL.reg
      = EVSYS_CHANNEL_CHANNEL(QUANTUM_EVENT_CHANNEL)
      | EVSYS_CHANNEL_EVGEN(EVSYS_ID_GEN_TC4_OVF)
      | EVSYS_CHANNEL_PATH_ASYNCHRONOUS
      ;
  }

  void stopQuantumEvents() {
    EVSYS->CHANNEL.reg
      = EVSYS_CHANNEL_CHANNEL(QUANTUM_EVENT_CHANNEL)
      | EVSYS_CHANNEL_EVGEN(0)
      | EVSYS_CHANNEL_PATH_ASYNCHRONOUS
      ;
  }

  void initializeEvents() {
    for ( uint16_t user :
          { EVSYS_ID_USER_TC5_EVU,
            EVSYS_ID_USER_TCC0_EV_0,
            EVSYS_ID_USER_TCC1_EV_0,
            EVSYS_ID_USER_TCC2_EV_0}
        ) {
      EVSYS->USER.reg
        = EVSYS_USER_USER(user)
        | EVSYS_USER_CHANNEL(QUANTUM_EVENT_CHANNEL + 1)
            // yes, +1 as per data sheet
        ;
    }
    startQuantumEvents();

    EVSYS->USER.reg
      = EVSYS_USER_USER(EVSYS_ID_USER_TCC0_MC_1)
      | EVSYS_USER_CHANNEL(EXTCLK_EVENT_CHANNEL + 1)
      ;
    EVSYS->CHANNEL.reg
      = EVSYS_CHANNEL_CHANNEL(EXTCLK_EVENT_CHANNEL)
      | EVSYS_CHANNEL_EVGEN(EVSYS_ID_GEN_EIC_EXTINT_8)
      | EVSYS_CHANNEL_PATH_ASYNCHRONOUS
      ;
  }


  Tc* const quantumTc = TC4;
  Tc* const beatTc = TC5;
  Tcc* const sequenceTcc = TCC0;
  Tcc* const measureTcc = TCC1;
  Tcc* const tupletTcc = TCC2;

  // NOTE: All TC units are used in 16 bit mode.

  void sync(Tc* tc) {
    while(tc->COUNT16.STATUS.bit.SYNCBUSY);
  }

  void disableAndReset(Tc* tc) {
    tc->COUNT16.CTRLA.reg &= ~TC_CTRLA_ENABLE;
    sync(tc);

    tc->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;
    sync(tc);
  }

  void enable(Tc* tc) {
    tc->COUNT16.CTRLA.reg |= TC_CTRLA_ENABLE;
    sync(tc);
  }

  void sync(Tcc* tcc, uint32_t mask) {
    while(tcc->SYNCBUSY.reg & mask);
  }

  void initializeTcc(Tcc* tcc) {
    tcc->CTRLA.bit.ENABLE = 0;
    sync(tcc, TCC_SYNCBUSY_ENABLE);

    tcc->CTRLA.bit.SWRST = 1;
    sync(tcc, TCC_SYNCBUSY_SWRST);

    tcc->CTRLA.bit.RUNSTDBY = 1;

    if (tcc == sequenceTcc) {
      tcc->CTRLA.bit.CPTEN1 = 1;
    }

    tcc->EVCTRL.reg
      = TCC_EVCTRL_TCEI0
      | TCC_EVCTRL_EVACT0_COUNTEV
      | ((tcc == sequenceTcc) ? TCC_EVCTRL_MCEI1 : 0)
      ;

    if (tcc == measureTcc) {
      tcc->INTENSET.reg = TCC_INTENSET_OVF;
    }
    if (tcc == sequenceTcc) {
      tcc->INTENSET.reg = TCC_INTENSET_MC1;
    }

    tcc->WEXCTRL.reg
      = TCC_WEXCTRL_OTMX(2);  // use CC[0] for all outputs

    tcc->WAVE.reg
      = TCC_WAVE_WAVEGEN_NPWM;
    sync(tcc, TCC_SYNCBUSY_WAVE);

    tcc->CTRLA.bit.ENABLE = 1;
    sync(tcc, TCC_SYNCBUSY_ENABLE);
  }

  void syncAll( uint32_t mask) {
    sync(measureTcc, mask);
    sync(sequenceTcc, mask);
    sync(tupletTcc, mask);
  }

  void readCounts(Timing& counts) {
    counts.measure = measureTcc->COUNT.reg;
    counts.sequence = sequenceTcc->COUNT.reg;
    counts.beat = qcast(beatTc->COUNT16.COUNT.reg);
    counts.tuplet = tupletTcc->COUNT.reg;
    syncAll(TCC_SYNCBUSY_COUNT);
    sync(beatTc);
  }

  void writeCounts(Timing& counts) {
    measureTcc->COUNT.reg = counts.measure;
    sequenceTcc->COUNT.reg = counts.sequence;
    beatTc->COUNT16.COUNT.reg = static_cast<uint16_t>(counts.beat);
    tupletTcc->COUNT.reg = counts.tuplet;
    syncAll(TCC_SYNCBUSY_COUNT);
    sync(beatTc);
  }

  void writePeriods(Timing& counts) {
    measureTcc->PER.reg = counts.measure;
    sequenceTcc->PER.reg = counts.sequence;
    beatTc->COUNT16.CC[0].reg = static_cast<uint16_t>(counts.beat);
    tupletTcc->PER.reg = counts.tuplet;
    syncAll(TCC_SYNCBUSY_PER);
    sync(beatTc);

    measureTcc->CC[0].reg = min(Q_PER_B, counts.measure) / 4;
    sequenceTcc->CC[0].reg = min(Q_PER_B, counts.sequence) / 4;
    beatTc->COUNT16.CC[1].reg = static_cast<uint16_t>(counts.beat / 4);
    tupletTcc->CC[0].reg = min(Q_PER_B, counts.tuplet) / 4;
    syncAll(TCC_SYNCBUSY_CC0);
    sync(beatTc);


  // TC4 has a 16 bit counter
  typedef uint16_t divisor_t;
  const double cpuFactor = 60.0 * F_CPU / Q_PER_B;

  bpm_t divisorToBpm(divisor_t divisor) {
    return round(cpuFactor / (double)divisor);
  }

  divisor_t divisorFromBpm(bpm_t bpm) {
    return round(cpuFactor / (double)bpm);
  }

  constexpr divisor_t divisorMin = 952;
  constexpr divisor_t divisorMax = 9524;


  divisor_t activeDivisor = 0;  // so it will be set the first time
  bpm_t activeBpm = 0;
  bool activeBpmValid = false;

  divisor_t externalDivisor = 0;

  void setDivisor(divisor_t divisor) {
    if (divisor == activeDivisor)
      return;

    // FIXME: would be better to adjust count to match remaining fration used
    // would be more efficient to keep previous divisor so no need to sync
    // read it back from the timer again
    quantumTc->COUNT16.CC[0].bit.CC = divisor;
    activeDivisor = divisor;
    activeBpmValid = false;
  }


  SyncMode captureSync = syncFixed;
  int capturesPerBeat = 0;

  const int captureBufferSize = 64;
  uint32_t captureBuffer[captureBufferSize];
  int captureNext = 0;
  uint32_t captureSum = 0;
  int captureCount = 0;


  const q_t qMod = 4 * Q_PER_B;
  q_t lastSample = 0;

  void capture(q_t sample) {
    if (capturesPerBeat == 0)     // not sync'd
      return;

    sample %= qMod;
    if (lastSample > 0) {
      q_t qdiff = ((sample + qMod - lastSample) % qMod) * capturesPerBeat;
      uint32_t dNext =
        ((uint32_t)activeDivisor * (uint32_t)qdiff / (uint32_t)Q_PER_B);
      dNext = constrain(dNext, divisorMin, divisorMax);

      captureSum += dNext;
      if (captureCount >= capturesPerBeat) {
        captureSum -= captureBuffer[captureNext];
      } else {
        captureCount += 1;
      }
      captureBuffer[captureNext] = dNext;
      captureNext = (captureNext + 1) % capturesPerBeat;

      uint32_t dFilt = captureSum / captureCount;
      externalDivisor = static_cast<divisor_t>(dNext);
      setDivisor(static_cast<divisor_t>(dFilt));
    }

    lastSample = sample;
  }

  void clearCapture(int perBeat) {
    capturesPerBeat = 0;
    captureNext = 0;
    captureSum = 0;
    captureCount = 0;
    lastSample = 0;
    capturesPerBeat = perBeat;
  }
}


bpm_t currentBpm() {
  if (!activeBpmValid) {
    activeBpm = divisorToBpm(activeDivisor);
    activeBpmValid = true;
  }
  return activeBpm;
}

void setBpm(bpm_t bpm) {
  bpm = constrain(bpm, bpmMin, bpmMax);
  setDivisor(divisorFromBpm(bpm));
}

void setSync(SyncMode sync) {
  if (captureSync == sync)
    return;
  captureSync = sync;

  int perBeat = 0;
  switch (sync) {
    case syncFixed: perBeat = 0; break;
    case syncTap: perBeat = 1; break;
    default:
      if (sync & syncExternalFlag) {
        perBeat = sync & syncPPQNMask;
      } else {
        perBeat = 0;
      }
  }

  clearCapture(perBeat);
}



void initializeTimers(const State& state, void (*onMeasure)()) {

  tcc1callback = onMeasure;


  /** POWER **/

  PM->APBAMASK.reg
    |= PM_APBAMASK_EIC;

  PM->APBCMASK.reg
    |= PM_APBCMASK_EVSYS
    | PM_APBCMASK_TCC0
    | PM_APBCMASK_TCC1
    | PM_APBCMASK_TCC2
    | PM_APBCMASK_TC4
    | PM_APBCMASK_TC5;


  /** CLOCKS **/

  for ( uint16_t id : { GCM_EIC, GCM_TCC0_TCC1, GCM_TCC2_TC3, GCM_TC4_TC5 }) {
    GCLK->CLKCTRL.reg
      = GCLK_CLKCTRL_CLKEN
      | GCLK_CLKCTRL_GEN_GCLK0
      | GCLK_CLKCTRL_ID(id);
    while (GCLK->STATUS.bit.SYNCBUSY);
  }


  /** EVENTS **/

  initializeEvents();


  /** EXTERNAL INTERRUPT **/

  EIC->CTRL.reg = EIC_CTRL_SWRST;
  while (EIC->STATUS.bit.SYNCBUSY);
  EIC->EVCTRL.reg
    |= EIC_EVCTRL_EXTINTEO8;
  EIC->CONFIG[1].bit.FILTEN0 = 1;
  EIC->CONFIG[1].bit.SENSE0 = EIC_CONFIG_SENSE0_RISE_Val;
  EIC->CTRL.reg = EIC_CTRL_ENABLE;
  while (EIC->STATUS.bit.SYNCBUSY);


  /** QUANTUM TIMER **/

  disableAndReset(quantumTc);

  quantumTc->COUNT16.CTRLA.reg
    = TC_CTRLA_MODE_COUNT16
    | TC_CTRLA_WAVEGEN_MFRQ
    | TC_CTRLA_PRESCALER_DIV1
    | TC_CTRLA_RUNSTDBY  // FIXME: should this be here?
    | TC_CTRLA_PRESCSYNC_GCLK
    ;
  sync(quantumTc);

  quantumTc->COUNT16.EVCTRL.reg
    = TC_EVCTRL_OVFEO
    ;

  setBpm(state.userBpm);
  setSync(state.syncMode);

  enable(quantumTc);


  /** BEAT TIMER **/

  disableAndReset(beatTc);

  beatTc->COUNT16.CTRLA.reg
    = TC_CTRLA_MODE_COUNT16
    | TC_CTRLA_WAVEGEN_MPWM
    | TC_CTRLA_PRESCALER_DIV1
    | TC_CTRLA_RUNSTDBY  // FIXME: should this be here?
    | TC_CTRLA_PRESCSYNC_GCLK
    ;
  sync(beatTc);

  beatTc->COUNT16.EVCTRL.reg
    = TC_EVCTRL_TCEI
    | TC_EVCTRL_EVACT_COUNT
    ;

  beatTc->COUNT16.CC[0].bit.CC = Q_PER_B;        // period
  beatTc->COUNT16.CC[1].bit.CC = Q_PER_B / 4;    // pulse width

  enable(beatTc);

  pinPeripheral(PIN_SPI_SCK, PIO_TIMER);
    // Despite the Adafruit pinout diagram, this is actuall pin 30
    // on the Feather M0 Express. It is SAMD21's port PB11, and
    // configuring it for function "E" (PIO_TIMER) maps it to TC5.WO1.


  initializeTcc(measureTcc);
  pinPeripheral(PAD_SPI_RX, PIO_TIMER);
  pinPeripheral(PAD_SPI_TX, PIO_TIMER);
  NVIC_SetPriority(TCC1_IRQn, 0);
  NVIC_EnableIRQ(TCC1_IRQn);

  initializeTcc(sequenceTcc);
  pinPeripheral(PIN_SPI_MOSI, PIO_TIMER_ALT);
  pinMode(PIN_A1, INPUT_PULLUP);
  pinPeripheral(PIN_A1, PIO_EXTINT);
  NVIC_SetPriority(TCC0_IRQn, 0);
  NVIC_EnableIRQ(TCC0_IRQn);

  initializeTcc(tupletTcc);
  pinPeripheral(PIN_SPI_MISO, PIO_TIMER);

  resetTimers(state.settings);
}

void resetTimers(const Settings& settings) {
  Timing periods;
  Timing counts = { 0, 0, 0 };

  computePeriods(settings, periods);

  stopQuantumEvents();
  writeCounts(counts);
  writePeriods(periods);
  startQuantumEvents();
}

void updateTimers(const Settings& settings) {
  Timing periods;
  Timing counts;

  computePeriods(settings, periods);

  stopQuantumEvents();
  readCounts(counts);
  adjustOffsets(periods, counts);
  writeCounts(counts);
  writePeriods(periods);
  startQuantumEvents();
}

void dumpTimer() {
  uint16_t quantumCount = quantumTc->COUNT16.COUNT.reg;
  uint16_t beatCount = beatTc->COUNT16.COUNT.reg;

  Serial.print("q count = ");
  Serial.print(quantumCount);
  Serial.print(", b count = ");
  Serial.println(beatCount);
}

void dumpCapture() {
  Serial.printf("captures per beat: %d\n", capturesPerBeat);
  Serial.printf("capture count: %d\n", captureCount);
  if (capturesPerBeat == 0)
    return;

  int32_t sum = 0;

  for (int i = 0; i < captureCount; ++i) {
    sum += captureBuffer[i];
    Serial.printf("[%2d] %8d\n", i, captureBuffer[i]);
  }

  if (captureCount > 0)
    Serial.printf("Sum: %8d, Average: %8d\n\n", sum, sum / captureCount);

  Serial.printf("captureSum = %d\n", captureSum);
  Serial.printf("captureNext = %d\n", captureNext);

  Serial.printf("active:   %d bpm - %d divisor\n",
    divisorToBpm(activeDivisor), activeDivisor);
  Serial.printf("external: %d bpm - %d divisor\n",
    divisorToBpm(externalDivisor), externalDivisor);
}

void TCC0_Handler() {
  if (TCC0->INTFLAG.reg & TCC_INTFLAG_MC1) {
    capture(TCC0->CC[1].reg);
  }
  TCC0->INTFLAG.reg = TCC_INTFLAG_MC1;    // writing 1 clears the flag
}

void TCC1_Handler() {
  if (TCC1->INTFLAG.reg & TCC_INTFLAG_OVF) {
    tcc1callback();
    TCC1->INTFLAG.reg = TCC_INTFLAG_OVF;  // writing 1 clears the flag
  }
}

#endif // __SAMD21__ and used TC and TCC units
