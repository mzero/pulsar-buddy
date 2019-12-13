#include "layout.h"

#include "display.h"
#include "state.h"
#include "ui_field.h"
#include "ui_memory.h"
#include "ui_music.h"
#include "ui_sync.h"


namespace {
  // NOTE: Be careful when adjusting the range of available
  // settings, because they can produce timing periods that
  // are too big for the timers.

  auto fieldBpm
    = SyncField(0, 0, 15, 32, userState());

  auto fieldNumberMeasures
    = ValueField<uint8_t>(18,  5, 12, 20,
        userSettings().numberMeasures,
        { 1, 2, 3, 4, 5, 6, 7, 8 }
        );

  auto fieldBeatsPerMeasure
    = ValueField<uint8_t>(39,  0, 24, 14,
        userSettings().beatsPerMeasure,
        { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 }
        );

  auto fieldBeatUnit
    = ValueField<uint8_t>(39, 17, 24, 14,
        userSettings().beatUnit,
        { 2, 4, 8, 16 }
        );

  auto commonTimeSignatures
    = TimeSignatureField(39, 14, 24, 3,
        fieldBeatsPerMeasure, fieldBeatUnit
        );

  auto pendingLoopIndicator
    = PendingIndicator(18, 0, pendingLoop);


  auto fieldTupletCount
    = ValueField<uint8_t>(66,  5, 12, 20,
        userSettings().tupletCount,
        { 2, 3, 4, 5, 6, 7, 8, 9 }
        );
  auto fieldTupletTime
    = ValueField<uint8_t>(84,  5, 12, 20,
        userSettings().tupletTime,
        { 2, 3, 4, 6, 8 }
        );

  auto commonTuplets
    = TupletRatioField(78, 5, 6, 20,
        fieldTupletCount, fieldTupletTime
        );

  auto fieldTupletUnit
    = BeatField(96, 2, 12, 28,
        userSettings().tupletUnit
        );

  auto pendingTupletIndicator
    = PendingIndicator(66, 0, pendingTuplet);


  auto fieldMemory
    = MemoryField(111, 5, 17, 23,
        userState().memoryIndex
        );

  auto pendingMemoryIndicator
    = PendingIndicator(111, 0, pendingMemory);


  const std::initializer_list<Field*> selectableFields =
    { &fieldBpm,
      &fieldNumberMeasures,
      &fieldBeatsPerMeasure,
      &commonTimeSignatures,
      &fieldBeatUnit,
      &fieldTupletCount,
      &commonTuplets,
      &fieldTupletTime,
      &fieldTupletUnit,
      &fieldMemory
    };

  int selectedFieldIndex = 1;   // start on number of measures

  void updateSelectedField(int dir) {
    selectedFieldIndex =
      constrain(selectedFieldIndex + dir, 0, (int)(selectableFields.size()) - 1);
  }

  Field* selectedField() {    // "class" needed due to auto-prototype gen.
    return selectableFields.begin()[selectedFieldIndex];
  }

  enum SelectMode { selectNone, selectField, selectValue };

  SelectMode selectMode = selectNone;
}

void resetSelection() {
  if (selectMode == selectValue)
    selectedField()->exit();

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

void clickSelection(ButtonState s) {
  switch (selectMode) {

    case selectNone:
      if (s == buttonUp || s == buttonUpLong) {
        selectMode = selectField;
        selectedField()->select();
      }
      break;

    case selectField:
      switch (s) {
        case buttonDownLong:
          selectedField()->enter(true);
          break;
        case buttonUp:
          selectedField()->enter(false);
          // fall through
        case buttonUpLong:
          selectMode = selectValue;
          break;
        default:
          break;
      }
      break;

    case selectValue:
      if (selectedField()->click(s)) {
        break;
      }
      if (s == buttonUp || s == buttonUpLong) {
        selectedField()->exit();
        selectMode = selectField;
      }
      break;
  }
}

namespace {

  void drawSeparator(int16_t x) {
    for (int16_t y = 0; y < 32; y += 2) {
      display.drawPixel(x, y, WHITE);
    }
  }
}

void drawAll(bool refresh) {
  display.dim(false);

  if (refresh) {
    display.clearDisplay();
    resetText();
  }

  // BPM area
  fieldBpm.render(refresh);

  if (refresh) drawSeparator(16);

  // Repeat area
  fieldNumberMeasures.render(refresh);
  if (refresh) {
    // the x
    display.drawLine(31, 12, 37, 18, WHITE);
    display.drawLine(31, 18, 37, 12, WHITE);
  }
  fieldBeatsPerMeasure.render(refresh);
  commonTimeSignatures.render(refresh);
  fieldBeatUnit.render(refresh);
  pendingLoopIndicator.render(refresh);

  if (refresh) drawSeparator(64);

  // Tuplet area
  fieldTupletCount.render(refresh);
  commonTuplets.render(refresh);
  fieldTupletTime.render(refresh);
  fieldTupletUnit.render(refresh);
  pendingTupletIndicator.render(refresh);

  if (refresh) drawSeparator(109);

  // Memory area
  fieldMemory.render(refresh);
  pendingMemoryIndicator.render(refresh);

  display.display();
}
