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

#define QUANTUM_EVENT_CHANNEL   5

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

    tcc->EVCTRL.reg
      = TCC_EVCTRL_TCEI0
      | TCC_EVCTRL_EVACT0_COUNTEV
      ;

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
  }

  uint16_t  CpuClockDivisor(double bpm) {
    static const double factor = 60.0 * F_CPU / Q_PER_B;
    return round(factor / bpm);
  }

}

void setTimerBpm(double bpm) {
  // FIXME: would be better to adjust count to match remaining fration used
  // would be more efficient to keep previous divisor so no need to sync
  // read it back from the timer again

  quantumTc->COUNT16.CC[0].bit.CC = CpuClockDivisor(bpm);
  sync(quantumTc);
}


void initializeTimers(double bpm) {

  /** POWER **/

  PM->APBCMASK.reg
    |= PM_APBCMASK_EVSYS
    | PM_APBCMASK_TCC0
    | PM_APBCMASK_TCC1
    | PM_APBCMASK_TCC2
    | PM_APBCMASK_TC4
    | PM_APBCMASK_TC5;


  /** CLOCKS **/

  for ( uint16_t id : { GCM_TCC0_TCC1, GCM_TCC2_TC3, GCM_TC4_TC5 }) {
    GCLK->CLKCTRL.reg
      = GCLK_CLKCTRL_CLKEN
      | GCLK_CLKCTRL_GEN_GCLK0
      | GCLK_CLKCTRL_ID(id);
    while (GCLK->STATUS.bit.SYNCBUSY);
  }


  /** EVENTS **/

  initializeEvents();

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

  setTimerBpm(bpm);

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

  pinMode(PIN_SPI_SCK, OUTPUT);
  pinPeripheral(PIN_SPI_SCK, PIO_TIMER);
    // Despite the Adafruit pinout diagram, this is actuall pin 30
    // on the Feather M0 Express. It is SAMD21's port PB11, and
    // configuring it for function "E" (PIO_TIMER) maps it to TC5.WO1.


  initializeTcc(measureTcc);
  pinMode(PAD_SPI_RX, OUTPUT);
  pinPeripheral(PAD_SPI_RX, PIO_TIMER);
  pinMode(PAD_SPI_TX, OUTPUT);
  pinPeripheral(PAD_SPI_TX, PIO_TIMER);

  initializeTcc(sequenceTcc);
  pinMode(PIN_SPI_MOSI, OUTPUT);
  pinPeripheral(PIN_SPI_MOSI, PIO_TIMER_ALT);

  initializeTcc(tupletTcc);
  pinMode(PIN_SPI_MISO, OUTPUT);
  pinPeripheral(PIN_SPI_MISO, PIO_TIMER);
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

#endif // __SAMD21__ and used TC and TCC units
