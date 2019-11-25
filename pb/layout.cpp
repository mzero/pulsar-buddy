#include "layout.h"

#include "display.h"
#include "state.h"
#include "ui_field.h"
#include "ui_music.h"


namespace {

  auto fieldNumberMeasures
    = ValueField<uint16_t>(18,  0, 10, 31,
        settings.numberMeasures,
        { 1, 2, 3, 4, 5, 6, 7, 8 }
        );

  auto fieldBeatsPerMeasure
    = ValueField<uint16_t>(38,  0, 24, 14,
        settings.beatsPerMeasure,
        { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 }
        );

  auto fieldBeatUnit
    = ValueField<uint16_t>(38, 17, 24, 14,
        settings.beatUnit,
        { 1, 2, 4, 8, 16 }
        );

  auto commonTimeSignatures
    = TimeSignatureField(38, 14, 24, 3,
        fieldBeatsPerMeasure, fieldBeatUnit
        );

  auto fieldTupletCount
    = ValueField<uint16_t>(66,  0, 12, 31,
        settings.tupletCount,
        { 2, 3, 4, 5, 6, 7, 8, 9 }
        );
  auto fieldTupletTime
    = ValueField<uint16_t>(84,  0, 12, 31,
        settings.tupletTime,
        { 2, 3, 4, 6, 8 }
        );

  auto commonTuplets
    = TupletRatioField(78, 0, 6, 31,
        fieldTupletCount, fieldTupletTime
        );

  auto fieldTupletUnit
    = BeatField(96, 2, 12, 28,
        settings.tupletUnit
        );

  const std::initializer_list<Field*> selectableFields =
    { &fieldNumberMeasures,
      &fieldBeatsPerMeasure,
      &commonTimeSignatures,
      &fieldBeatUnit,
      &fieldTupletCount,
      &commonTuplets,
      &fieldTupletTime,
      &fieldTupletUnit
    };

  int selectedFieldIndex = 0;
  void updateSelectedField(int dir) {
    selectedFieldIndex =
      constrain(selectedFieldIndex + dir, 0, selectableFields.size() - 1);
  }

  Field* selectedField() {    // "class" needed due to auto-prototype gen.
    return selectableFields.begin()[selectedFieldIndex];
  }

  enum SelectMode { selectNone, selectField, selectValue };

  SelectMode selectMode = selectNone;

}

void resetSelection() {
  selectMode = selectNone;
  selectedField()->deselect();
}

void updateSelection(int dir) {
  switch (selectMode) {

    case selectNone:
      selectMode = selectField;
      selectedField()->select();
      break;

    case selectField:
      selectedField()->deselect();
      updateSelectedField(dir);
      selectedField()->select();
      break;

    case selectValue:
      selectedField()->update(dir);
      break;
  }
}

void clickSelection() {
  switch (selectMode) {

    case selectNone:
      selectMode = selectField;
      selectedField()->select();
      break;

    case selectField:
      selectMode = selectValue;
      break;

    case selectValue:
      selectMode = selectField;
      break;
  }
}

namespace {

  void drawBPM() {
    display.setRotation(3);
    display.fillRect(0, 0, 32, 14, BLACK);
    display.setTextColor(WHITE);
    centerNumber(bpm, 0, 0, 32, 14);
    display.setRotation(0);
  }

  void drawSeparator(int16_t x) {
    for (int16_t y = 0; y < 32; y += 2) {
      display.drawPixel(x, y, WHITE);
    }
  }

  void drawRepeat() {
    fieldNumberMeasures.render();
    fieldBeatsPerMeasure.render();
    commonTimeSignatures.render();
    fieldBeatUnit.render();
  }

  void drawTuplet() {
    fieldTupletCount.render();
    commonTuplets.render();
    fieldTupletTime.render();
    fieldTupletUnit.render();
  }

  void drawMemory() {
    display.fillCircle(115, 11, 3, WHITE);
    display.fillCircle(115, 11, 2, BLACK);
    display.fillCircle(115, 21, 3, WHITE);
    display.fillCircle(123, 11, 3, WHITE);
    display.fillCircle(123, 21, 3, WHITE);
  }

  void drawFixed() {
    // BPM area

    drawSeparator(16);

    // Repeat area
    //    the x
    display.drawLine(30, 12, 36, 18, WHITE);
    display.drawLine(30, 18, 36, 12, WHITE);

    drawSeparator(64);

    // Tuplet area

    drawSeparator(109);

    // Memory area
  }

}

void drawAll(bool refresh) {
  display.dim(false);

  unsigned long t0 = millis();

  if (refresh) {
    display.clearDisplay();

    resetText();
    drawFixed();
  }

  drawBPM();
  drawRepeat();
  drawTuplet();
  drawMemory();

  unsigned long t1 = millis();

  display.display();

  unsigned long t2 = millis();
  Serial.print("draw ms: ");
  Serial.print(t1 - t0);
  Serial.print(", disp ms: ");
  Serial.println(t2 - t1);
}