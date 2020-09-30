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

  q_t qForWidth(PulseWidth pt, q_t period) {
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
  Serial.print("  sequence = "); dumpQ(t.sequence);  Serial.println();
  Serial.print("  measure  = "); dumpQ(t.measure);   Serial.println();
  Serial.println();
  Serial.print("  periodS  = "); dumpQ(t.periodS);   Serial.println();
  Serial.print("  periodM  = "); dumpQ(t.periodM);   Serial.println();
  Serial.print("  periodB  = "); dumpQ(t.periodB);   Serial.println();
  Serial.print("  periodT  = "); dumpQ(t.periodT);   Serial.println();
  Serial.println();
  Serial.print("  widthS   = "); dumpQ(t.widthS);    Serial.println();
  Serial.print("  widthM   = "); dumpQ(t.widthM);    Serial.println();
  Serial.print("  widthB   = "); dumpQ(t.widthB);    Serial.println();
  Serial.print("  widthT   = "); dumpQ(t.widthT);    Serial.println();
}

void dumpOffsets(const Offsets& t) {
  Serial.print("  countS   = "); dumpQ(t.countS);    Serial.println();
  Serial.print("  countM   = "); dumpQ(t.countM);    Serial.println();
  Serial.print("  countB   = "); dumpQ(t.countB);    Serial.println();
  Serial.print("  countT   = "); dumpQ(t.countT);    Serial.println();
}


void computePeriods(const State& s, Timing& t) {
  const Settings& u = s.settings;

  auto measure  = qcast(u.beatsPerMeasure) * qPerBeatUnit(u.beatUnit);
  auto sequence = qcast(u.numberMeasures) * measure;
  auto beat     = qPerBeatUnit(u.tupletUnit);
  auto tuplet   = qcast(u.tupletTime) * beat / qcast(u.tupletCount);

  t.sequence  = sequence;
  t.measure   = measure;

  t.periodS   = sequence;
  t.periodM   = measure;
  t.periodB   = beat;
  t.periodT   = tuplet;

  t.widthS    = qForWidth(s.pulseWidthS,  t.periodS);
  t.widthM    = qForWidth(s.pulseWidthM,  t.periodM);
  t.widthB    = qForWidth(s.pulseWidthB,  t.periodB);
  t.widthT    = qForWidth(s.pulseWidthT,  t.periodT);

  if (configuration.debug.timing) {
    Serial.println("computed new periods:");
    dumpTiming(t);
  }
}

void setOffsets(const Timing& t, q_t position, Offsets& offsets) {
  q_t now = position % t.sequence;

  offsets.countS = now % t.periodS;
  offsets.countM = now % t.periodM;
  offsets.countB = now % t.periodB;
  offsets.countT = now % t.periodT;

  if (configuration.debug.timing) {
    Serial.println("setting offsets to:");
    dumpOffsets(offsets);
  }
}

void adjustOffsets(const Timing& t, Offsets& offsets) {
  if (configuration.debug.timing) {
    Serial.println("adjusting these offsets:");
    dumpOffsets(offsets);
  }

  setOffsets(t, offsets.countS, offsets);
}


