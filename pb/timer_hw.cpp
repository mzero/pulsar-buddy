#include "timer_hw.h"

#include <Arduino.h>
#include <wiring_private.h>

// *** NOTE: This implementation is for the SAMD21 only.
#if defined(__SAMD21__) || defined(TC4) || defined(TC5)

/* Timer Diagram:

  QUANTUM --> event --+--> BEAT --> waveform
   (TC4)              |    (TC5)
                      |
                      +--> RESET --> waveform
                      |    (TCC0)
                      |
                      +--> MEASURE --> waveform
                      |    (TCC1)
                      |
                      +--> TUPLET --> waveform
                           (TCC0)

*/

namespace {

  Tc* const quantumTc = TC4;
  Tc* const beatTc = TC5;


  // NOTE: All TC units are used in 16 bit mode.

  inline void sync(Tc* tc) {
    while(tc->COUNT16.STATUS.bit.SYNCBUSY);
  }

  inline void disableAndReset(Tc* tc) {
    tc->COUNT16.CTRLA.reg &= ~TC_CTRLA_ENABLE;
    sync(tc);

    tc->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;
    sync(tc);
  }

  inline void enable(Tc* tc) {
    tc->COUNT16.CTRLA.reg |= TC_CTRLA_ENABLE;
    sync(tc);
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

  uint16_t divisor = CpuClockDivisor(bpm);
  quantumTc->COUNT16.CC[0].bit.CC = divisor;
  sync(quantumTc);

  Serial.print(round(bpm));
  Serial.print("bpm => divisor ");
  Serial.println(divisor);
}


void initializeTimers(double bpm) {

  /** POWER **/

  PM->APBCMASK.reg
    |= PM_APBCMASK_EVSYS
    | PM_APBCMASK_TC4
    | PM_APBCMASK_TC5;


  /** CLOCKS **/

  GCLK->CLKCTRL.reg
    = GCLK_CLKCTRL_CLKEN
    | GCLK_CLKCTRL_GEN_GCLK0
    | GCLK_CLKCTRL_ID(GCM_TC4_TC5);
  while (GCLK->STATUS.bit.SYNCBUSY);


  /** EVENTS **/

#define QUANTUM_EVENT_CHANNEL   5

  EVSYS->USER.reg
    = EVSYS_USER_USER(EVSYS_ID_USER_TC5_EVU)
    | EVSYS_USER_CHANNEL(QUANTUM_EVENT_CHANNEL + 1)
        // yes, +1 as per data sheet
    ;
  EVSYS->CHANNEL.reg
    = EVSYS_CHANNEL_CHANNEL(QUANTUM_EVENT_CHANNEL)
    | EVSYS_CHANNEL_EVGEN(EVSYS_ID_GEN_TC4_OVF)
    | EVSYS_CHANNEL_PATH_ASYNCHRONOUS
    ;


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
  Serial.println(PIN_SPI_SCK);

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
