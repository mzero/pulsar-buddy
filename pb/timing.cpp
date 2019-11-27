#include "timing.h"

#include <Arduino.h>


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
    Serial.print("** Unknown beat unit: ");
    Serial.println(b);
    return Q_PER_B;
  }

}

void computePeriods(const Settings& s, Timing& p) {
  p.measure = qcast(s.beatsPerMeasure) * qPerBeatUnit(s.beatUnit);
  p.sequence = qcast(s.numberMeasures) * p.measure;
  p.beat = qPerBeatUnit(s.tupletUnit);
  p.tuplet = qcast(s.tupletTime) * p.beat / qcast(s.tupletCount);
}

void adjustOffsets(const Timing& periods, Timing& offsets) {
  q_t sequence = offsets.sequence % periods.sequence;

  offsets.sequence = sequence % periods.sequence;
  offsets.measure = sequence % periods.measure;
  offsets.beat = sequence % periods.beat;
  offsets.tuplet = sequence % periods.tuplet;
}

