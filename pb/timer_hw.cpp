#include "timer_hw.h"

#include <initializer_list>

#include <Arduino.h>

#include "pins.h"


namespace {
  const float cpuFactor = 60.0f * F_CPU / Q_PER_B;
}

float divisorToBpm(divisor_t divisor) {
  return cpuFactor / (float)divisor;
}

divisor_t divisorFromBpm(float bpm) {
  return (divisor_t)(roundf(cpuFactor / bpm));
}

namespace {

  const uint32_t cpuTicksInMinWidth = F_CPU / 1000 * 2667 / 1000;
    // ticks_sec / ms_per_sec * width_in_micros / micros_per_ms
    // Becareful to keep this calc. from over- or under- flowing!

  q_t divisorToMinWidth(divisor_t divisor) {
    return max(6, divisor ? cpuTicksInMinWidth / divisor : 0);
      // handle zero divisor at start, and also make sure always sane
  }
}


// *** NOTE: This implementation is for the SAMD21 only.
#if defined(__SAMD21__) || defined(TC4) || defined(TC5)

/* Timer Diagram:

  QUANTUM -+-> event --+--> BEAT --> waveform
   (TC4)   |           |    (TC5)
           |           |
           |           +--> SEQUENCE --> waveform
           |           |    (TCC0)
           |           |
           |           +--> MEASURE --> waveform
           |           |    (TCC1)
           |           |
           |           +--> TUPLET --> waveform
           |                (TCC2)
           |
           +-> event ----> WATCHDOG --> timeout
                           (TC3)

  Note on synchonization:

  TC units: No sync is required by the MCU as the bus and CPU will just stall
    until things are ready. This is fine. The only sync done here is to ensure
    that something is complete before proceeding.

  TCC units: Sync is required because access while not sync'd will cause a HW
    interrupt. Post write sync isn't required, except to ensure completion
    before proceeding.

  Note: Do not reassign which timer does what without consulting pins.cpp.
*/

namespace {

#define QUANTUM_A_EVENT_CHANNEL   5
#define QUANTUM_B_EVENT_CHANNEL   6
#define EXTCLK_EVENT_CHANNEL      7


  ExtClockSource currentExtClockSource;

  void setExtClockSource(ExtClockSource s) {
    switch (s) {
      case extClockHardware:
        EVSYS->CHANNEL.reg
          = EVSYS_CHANNEL_CHANNEL(EXTCLK_EVENT_CHANNEL)
          | EVSYS_CHANNEL_EVGEN(EVSYS_ID_GEN_EIC_EXTINT_8)
          | EVSYS_CHANNEL_PATH_ASYNCHRONOUS
          ;
        break;

      case extClockSoftware:
        EVSYS->CHANNEL.reg
          = EVSYS_CHANNEL_CHANNEL(EXTCLK_EVENT_CHANNEL)
          | EVSYS_CHANNEL_PATH_SYNCHRONOUS
          | EVSYS_CHANNEL_EDGSEL_RISING_EDGE
          ;
        break;
    }
    currentExtClockSource = s;
  }

  void initializeEvents() {
    EVSYS->CTRL.bit.GCLKREQ = 1;
      // for when ext clks are generated from software

    for ( uint16_t user :
          { EVSYS_ID_USER_TC5_EVU,
            EVSYS_ID_USER_TCC0_EV_0,
            EVSYS_ID_USER_TCC1_EV_0,
            EVSYS_ID_USER_TCC2_EV_0}
        ) {
      EVSYS->USER.reg
        = EVSYS_USER_USER(user)
        | EVSYS_USER_CHANNEL(QUANTUM_A_EVENT_CHANNEL + 1)
            // yes, +1 as per data sheet
        ;
    }

    EVSYS->USER.reg
      = EVSYS_USER_USER(EVSYS_ID_USER_TC3_EVU)
      | EVSYS_USER_CHANNEL(QUANTUM_B_EVENT_CHANNEL + 1)
      ;
    EVSYS->CHANNEL.reg
      = EVSYS_CHANNEL_CHANNEL(QUANTUM_B_EVENT_CHANNEL)
      | EVSYS_CHANNEL_EVGEN(EVSYS_ID_GEN_TC4_OVF)
      | EVSYS_CHANNEL_PATH_ASYNCHRONOUS
      ;

    EVSYS->USER.reg
      = EVSYS_USER_USER(EVSYS_ID_USER_TCC0_MC_1)
      | EVSYS_USER_CHANNEL(EXTCLK_EVENT_CHANNEL + 1)
      ;

    setExtClockSource(extClockHardware);
  }

  bool quantumRunning = false;
}

void startQuantumEvents() {
  if (!quantumRunning) {
    EVSYS->CHANNEL.reg
      = EVSYS_CHANNEL_CHANNEL(QUANTUM_A_EVENT_CHANNEL)
      | EVSYS_CHANNEL_EVGEN(EVSYS_ID_GEN_TC4_OVF)
      | EVSYS_CHANNEL_PATH_ASYNCHRONOUS
      ;
    quantumRunning = true;
  }
}

void stopQuantumEvents() {
  if (quantumRunning) {
    EVSYS->CHANNEL.reg
      = EVSYS_CHANNEL_CHANNEL(QUANTUM_A_EVENT_CHANNEL)
      | EVSYS_CHANNEL_EVGEN(0)
      | EVSYS_CHANNEL_PATH_ASYNCHRONOUS
      ;
    quantumRunning = false;
  }
}

PauseQuantum::PauseQuantum()
  : wasRunning(quantumRunning)
  { if (wasRunning) stopQuantumEvents(); }

PauseQuantum::~PauseQuantum()
  { if (wasRunning) startQuantumEvents(); }


void useExtClockSource(ExtClockSource s) {
  if (s != currentExtClockSource)
    setExtClockSource(s);
}

void softwareExtClock() {
  if (currentExtClockSource == extClockSoftware) {
    // According to the data sheet, this must be done with a 16-bit write.
    // However, the data structures for EVSYS only have 32-bit register fields.
    // Hence the following cast:
    *(reinterpret_cast<volatile uint16_t*>(&EVSYS->CHANNEL.reg))
        = EVSYS_CHANNEL_CHANNEL(EXTCLK_EVENT_CHANNEL)
        | EVSYS_CHANNEL_SWEVT
        ;
  }
}




namespace {
  Tc* const quantumTc = TC4;
  Tc* const watchdogTc = TC3;
  Tc* const beatTc = TC5;
  Tcc* const sequenceTcc = TCC0;
  Tcc* const measureTcc = TCC1;
  Tcc* const tupletTcc = TCC2;


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
    sync(tc);   // make sure all changes are sync'd before enabling
    tc->COUNT16.CTRLA.reg |= TC_CTRLA_ENABLE;
    sync(tc);
  }

  inline void sync(Tcc* tcc, uint32_t mask) {
    while(tcc->SYNCBUSY.reg & mask);
  }

  void initializeTcc(Tcc* tcc) {
    sync(tcc, TCC_SYNCBUSY_ENABLE);
    tcc->CTRLA.bit.ENABLE = 0;
    sync(tcc, TCC_SYNCBUSY_ENABLE);

    sync(tcc, TCC_SYNCBUSY_SWRST);
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

    if (tcc == sequenceTcc) {
      tcc->INTENSET.reg =
        TCC_INTENSET_OVF
        | TCC_INTENSET_MC1 | TCC_INTENSET_MC2 | TCC_INTENSET_MC3;
    }

    tcc->WEXCTRL.reg
      = TCC_WEXCTRL_OTMX(2);  // use CC[0] for all outputs

    tcc->DRVCTRL.reg
      = TCC_DRVCTRL_INVEN0
      | TCC_DRVCTRL_INVEN1
      | TCC_DRVCTRL_INVEN2
      | TCC_DRVCTRL_INVEN3
      | TCC_DRVCTRL_INVEN4
      | TCC_DRVCTRL_INVEN5
      | TCC_DRVCTRL_INVEN6
      | TCC_DRVCTRL_INVEN7;
      // inverted, because output buffer swaps

    sync(tcc, TCC_SYNCBUSY_WAVE);
    tcc->WAVE.reg
      = TCC_WAVE_WAVEGEN_NPWM;
    sync(tcc, TCC_SYNCBUSY_WAVE);

    sync(tcc, TCC_SYNCBUSY_ENABLE);
    tcc->CTRLA.bit.ENABLE = 1;
    sync(tcc, TCC_SYNCBUSY_ENABLE);
  }

  q_t activeSequence;
  q_t activeMeasure;


  inline q_t nextInterval(q_t interval, q_t currSequence) {
    auto m =
      interval
        ? (currSequence - currSequence % interval) + interval
        : 0;
    if (m >= activeSequence)
      m = 0;
        // zero doesn't work for CC match, BUT interrupt on overflow is
        // effectively the same thing, so it is caught.
    return m;
  }

  inline q_t nextMeasure(q_t currSequence) {
    return nextInterval(activeMeasure, currSequence);
  }

  inline q_t nextMidiClock(q_t currSequence) {
    return nextInterval(Q_PER_MIDI_CLOCK, currSequence);
  }
}


void setQuantumDivisor(divisor_t d) {
  quantumTc->COUNT16.CC[0].reg = (divisor_t)(d - 1);
}


void readCounts(Offsets& counts) {
  // request read sync, with command written to ctrlb
  sync(sequenceTcc, TCC_SYNCBUSY_CTRLB);
  sync(measureTcc, TCC_SYNCBUSY_CTRLB);
  sync(tupletTcc, TCC_SYNCBUSY_CTRLB);
  sequenceTcc->CTRLBSET.reg = TCC_CTRLBSET_CMD_READSYNC;
  measureTcc->CTRLBSET.reg = TCC_CTRLBSET_CMD_READSYNC;
  tupletTcc->CTRLBSET.reg = TCC_CTRLBSET_CMD_READSYNC;

  // wait for command to complete
  while (sequenceTcc->CTRLBSET.bit.CMD);
  while (measureTcc->CTRLBSET.bit.CMD);
  while (tupletTcc->CTRLBSET.bit.CMD);

  // sync for count (probably redudant with the above?)
  sync(sequenceTcc, TCC_SYNCBUSY_COUNT);
  sync(measureTcc, TCC_SYNCBUSY_COUNT);
  sync(tupletTcc, TCC_SYNCBUSY_COUNT);
  counts.countS = sequenceTcc->COUNT.reg;
  counts.countM = measureTcc->COUNT.reg;
  counts.countB = qcast(beatTc->COUNT16.COUNT.reg);
  counts.countT = tupletTcc->COUNT.reg;
}

void writeCounts(const Offsets& counts) {
  // sync as a group - though quantum is stopped, so shouldn't matter
  sync(sequenceTcc, TCC_SYNCBUSY_COUNT | TCC_SYNCBUSY_CC2 | TCC_SYNCBUSY_CC3);
  sync(measureTcc, TCC_SYNCBUSY_COUNT);
  sync(tupletTcc, TCC_SYNCBUSY_COUNT);

  sequenceTcc->COUNT.reg = counts.countS;
  measureTcc->COUNT.reg = counts.countM;
  beatTc->COUNT16.COUNT.reg = static_cast<uint16_t>(counts.countB);
  tupletTcc->COUNT.reg = counts.countT;

  sequenceTcc->CC[2].reg = nextMeasure(counts.countS);
  sequenceTcc->CC[3].reg = nextMidiClock(counts.countS);
}


namespace {
  q_t lastBeatWidth = 0;
}

void writePeriods(const Timing& timing, divisor_t divisor) {
  activeSequence = timing.sequence;
  activeMeasure = timing.measure;

  // sync as a group - though quantum is stopped, so shouldn't matter
  sync(sequenceTcc, TCC_SYNCBUSY_PER | TCC_SYNCBUSY_CC0);
  sync(measureTcc, TCC_SYNCBUSY_PER | TCC_SYNCBUSY_CC0);
  sync(tupletTcc, TCC_SYNCBUSY_PER | TCC_SYNCBUSY_CC0);

  sequenceTcc->PER.reg = timing.periodS - 1;
  measureTcc->PER.reg = timing.periodM - 1;
  beatTc->COUNT16.CC[0].reg = static_cast<uint16_t>(timing.periodB - 1);
  tupletTcc->PER.reg = timing.periodT - 1;


  q_t minQ = divisorToMinWidth(divisor);

  sequenceTcc->CCB[0].reg   = max(minQ, timing.widthS) - 1;
  measureTcc->CCB[0].reg    = max(minQ, timing.widthM) - 1;
  beatTc->COUNT16.CC[1].reg =
    lastBeatWidth           = max(minQ, timing.widthB) - 1;
  tupletTcc->CCB[0].reg     = max(minQ, timing.widthT) - 1;
}

void updateWidths(divisor_t divisor, const Timing& timing) {
  q_t minQ = divisorToMinWidth(divisor);

  // TCC versions are buffered, changes on next cycle, don't need sync
  // TC version happens immediately, and will stall if sync'ing

  sequenceTcc->CCB[0].reg   = max(minQ, timing.widthS) - 1;
  measureTcc->CCB[0].reg    = max(minQ, timing.widthM) - 1;

  q_t beatWidth = max(minQ, timing.widthB) - 1;
  if (beatWidth != lastBeatWidth) {
      // avoid writing (and stalling for sync) if not changed
      beatTc->COUNT16.CC[1].reg = lastBeatWidth = beatWidth;
  }

  tupletTcc->CCB[0].reg     = max(minQ, timing.widthT) - 1;
}


namespace {

  void initializePins() {
    TriggerOutput::S.initialize();
    TriggerOutput::M.initialize();
    TriggerOutput::B.initialize();
    TriggerOutput::T.initialize();

    disableTriggers(true, true);

    TriggerInput::C.initialize();
    TriggerInput::O.initialize();   // though otherwise unused
  }
}

void disableTriggers(bool off, bool force) {
  static bool currentlyOff = false;

  if (!force && off == currentlyOff) return;
  currentlyOff = off;

  TriggerOutput::S.disable(off);
  TriggerOutput::M.disable(off);
  TriggerOutput::B.disable(off);
  TriggerOutput::T.disable(off);
}


q_t resetWatchdog(q_t interval)
{
    q_t resetCount = constrain(0x10000 - interval, 0, 0xffff);
    watchdogTc->COUNT16.COUNT.reg = (uint16_t)(resetCount);
    return resetCount;
}


void initializeTimers() {
  /** POWER **/

  PM->APBAMASK.reg
    |= PM_APBAMASK_EIC;

  PM->APBCMASK.reg
    |= PM_APBCMASK_EVSYS
    | PM_APBCMASK_TCC0
    | PM_APBCMASK_TCC1
    | PM_APBCMASK_TCC2
    | PM_APBCMASK_TC3
    | PM_APBCMASK_TC4
    | PM_APBCMASK_TC5;


  /** CLOCKS **/

  for ( uint16_t id :
    { GCM_EIC, GCM_EVSYS_CHANNEL_7, GCM_TCC0_TCC1, GCM_TCC2_TC3, GCM_TC4_TC5 })
  {
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
  EIC->CONFIG[1].bit.SENSE0 = EIC_CONFIG_SENSE0_FALL_Val;
    // input buffer circuit inverts the signal
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

  quantumTc->COUNT16.EVCTRL.reg
    = TC_EVCTRL_OVFEO
    ;

  enable(quantumTc);


  /** WATCH DOG TIMER **/

  disableAndReset(watchdogTc);

  watchdogTc->COUNT16.CTRLA.reg
    = TC_CTRLA_MODE_COUNT16
    | TC_CTRLA_PRESCALER_DIV1
    | TC_CTRLA_RUNSTDBY  // FIXME: should this be here?
    | TC_CTRLA_PRESCSYNC_GCLK
    ;

  watchdogTc->COUNT16.EVCTRL.reg
    = TC_EVCTRL_TCEI
    | TC_EVCTRL_EVACT_COUNT
    ;

  watchdogTc->COUNT16.INTENSET.reg
    = TC_INTENSET_OVF
    ;

  enable(watchdogTc);

  NVIC_SetPriority(TC3_IRQn, 0);
  NVIC_EnableIRQ(TC3_IRQn);

  /** BEAT TIMER **/

  disableAndReset(beatTc);

  beatTc->COUNT16.CTRLA.reg
    = TC_CTRLA_MODE_COUNT16
    | TC_CTRLA_WAVEGEN_MPWM
    | TC_CTRLA_PRESCALER_DIV1
    | TC_CTRLA_RUNSTDBY  // FIXME: should this be here?
    | TC_CTRLA_PRESCSYNC_GCLK
    ;

  beatTc->COUNT16.CTRLC.reg
    = TC_CTRLC_INVEN0
    | TC_CTRLC_INVEN1;

  beatTc->COUNT16.EVCTRL.reg
    = TC_EVCTRL_TCEI
    | TC_EVCTRL_EVACT_COUNT
    ;

  enable(beatTc);

  initializeTcc(measureTcc);

  initializeTcc(sequenceTcc);
  NVIC_SetPriority(TCC0_IRQn, 0);
  NVIC_EnableIRQ(TCC0_IRQn);

  initializeTcc(tupletTcc);

  initializePins();
}


void dumpTimers() {
  Offsets counts;

  Serial.printf("quantum events %s\n",
    quantumRunning ? "running" : "stopped");

  {
    PauseQuantum pq;
    readCounts(counts);
  }

  dumpOffsets(counts);

  uint16_t watchCount = watchdogTc->COUNT16.COUNT.reg;
  Serial.printf("watchdogTimer count: %d\n", watchCount);
}




void TCC0_Handler() {
  auto intflag = TCC0->INTFLAG.reg;
  if (intflag & TCC_INTFLAG_MC1) {
    sync(sequenceTcc, TCC_SYNCBUSY_CC1);
    auto sequenceCapture = sequenceTcc->CC[1].reg;
    isrClockCapture(sequenceCapture);
  }
  if (intflag & (TCC_INTFLAG_OVF | TCC_INTFLAG_MC2 | TCC_INTFLAG_MC3)) {
    sync(sequenceTcc, TCC_SYNCBUSY_CTRLB);
    sequenceTcc->CTRLBSET.reg = TCC_CTRLBSET_CMD_READSYNC;
    while (sequenceTcc->CTRLBSET.bit.CMD);
    sync(sequenceTcc, TCC_SYNCBUSY_COUNT); // redudant with the above?
    auto s = sequenceTcc->COUNT.reg;

    if (intflag & (TCC_INTFLAG_OVF | TCC_INTFLAG_MC2)) {
      auto m = nextMeasure(s);
      sync(sequenceTcc, TCC_SYNCBUSY_CC2);
      sequenceTcc->CC[2].reg = m;

      isrMeasure();
    }
    if (false)  // TODO: enable this code
    if (intflag & (TCC_INTFLAG_OVF | TCC_INTFLAG_MC3)) {
      auto c = nextMidiClock(s);
      sync(sequenceTcc, TCC_SYNCBUSY_CC3);
      sequenceTcc->CC[3].reg = c;

      isrMidiClock();
    }
  }
  TCC0->INTFLAG.reg =
    TCC_INTFLAG_OVF | TCC_INTFLAG_MC1 | TCC_INTFLAG_MC2 | TCC_INTFLAG_MC3;
}

void TC3_Handler() {
  if (TC3->COUNT16.INTFLAG.reg & TC_INTFLAG_OVF) {
    isrWatchdog();
    TC3->COUNT16.INTFLAG.reg = TC_INTFLAG_OVF;   // writing 1 clears the flag
  }
}


#endif // __SAMD21__
