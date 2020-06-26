#include "timing.h"

#include <Arduino.h>

#include "config.h"
#include "critical.h"


namespace {

  q_t qPerBeatUnit(uint16_t b) {
    switch (b) {
      case  1: return Q_PER_B * 4;  // whole note
      case  2: return Q_PER_B * 2;  // half note
      case  4: return Q_PER_B;      // quarter note
      case  8: return Q_PER_B / 2;  // eighth note
      case 16: return Q_PER_B / 4;  // sixteenth note
    }

    // should never happen!
    critical.printf("Unsupported beat unit: %d\n", b);
    return Q_PER_B;
  }

  q_t qForWidth(PulseType pt, q_t period) {
    switch (pt) {
      case pulseFixedShort:   return 0;
      case pulseDutyHalf:     return period / 2;
      case pulseDutyThird:    return period / 3;
      case pulseDutyQuarter:  return period / 4;
      case pulseDuration16:   return min(Q_PER_B, period) / 4;
      case pulseDuration32:   return min(Q_PER_B, period) / 8;
    }

    // should never happen!
    critical.printf("Unsupported pulse type: %d\n", pt);
    return 0;
  }
}

void dumpQ(q_t q){
  static const q_t Q_PER_16TH = Q_PER_B / 4;

  q_t x = q;
  q_t beats      = x / Q_PER_B;     x -= beats * Q_PER_B;
  q_t sixteenths = x / Q_PER_16TH;  x -= sixteenths * Q_PER_16TH;

  Serial.printf("%8dq = %3d/4 + %d/16 + %dq", q, beats, sixteenths, x);
}

void dumpTiming(const Timing& t) {
  Serial.print("  measure  = "); dumpQ(t.measure);   Serial.println();
  Serial.print("  sequence = "); dumpQ(t.sequence);  Serial.println();
  Serial.print("  beat     = "); dumpQ(t.beat);      Serial.println();
  Serial.print("  tuplet   = "); dumpQ(t.tuplet);    Serial.println();
}


void computePeriods(const Settings& s, Timing& p, Timing& w) {
  p.measure   = qcast(s.beatsPerMeasure) * qPerBeatUnit(s.beatUnit);
  p.sequence  = qcast(s.numberMeasures) * p.measure;
  p.beat      = qPerBeatUnit(s.tupletUnit);
  p.tuplet    = qcast(s.tupletTime) * p.beat / qcast(s.tupletCount);

  w.measure   = qForWidth(pulseDuration16,  p.measure);
  w.sequence  = qForWidth(pulseDuration16,  p.sequence);
  w.beat      = qForWidth(pulseDuration16,  p.beat);
  w.tuplet    = qForWidth(pulseDuration16,  p.tuplet);

  if (configuration.debug.timing) {
    Serial.println("computed new periods:");
    dumpTiming(p);
  }
}

void adjustOffsets(const Timing& periods, Timing& offsets) {
  q_t sequence = offsets.sequence % periods.sequence;

  if (configuration.debug.timing) {
    Serial.println("old offsets:");
    dumpTiming(offsets);
  }

  offsets.sequence = sequence % periods.sequence;
  offsets.measure = sequence % periods.measure;
  offsets.beat = sequence % periods.beat;
  offsets.tuplet = sequence % periods.tuplet;

  if (configuration.debug.timing) {
    Serial.println("new offsets:");
    dumpTiming(offsets);
  }
}

