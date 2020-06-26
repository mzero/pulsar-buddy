#include "layout.h"

#include "display.h"
#include "state.h"
#include "ui_field.h"
#include "ui_layout.h"
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


    class MainPage : public Layout {
    public:
      MainPage() : Layout(selectableFields, 1) { }
    protected:
      void redraw();
    };


    MainPage layout;
}

void resetSelection() {
  layout.exit();
}

void updateSelection(Encoder::Update update) {
  layout.update(update);
}

void clickSelection(Button::State s) {
  layout.click(s);
}

namespace {

  void drawSeparator(int16_t x) {
    for (int16_t y = 0; y < 32; y += 2) {
      display.drawPixel(x, y, WHITE);
    }
  }

  void MainPage::redraw() {
    drawSeparator(16);
    drawSeparator(64);
    drawSeparator(109);
    // the x
    display.drawLine(31, 12, 37, 18, WHITE);
    display.drawLine(31, 18, 37, 12, WHITE);
  }
}


bool drawAll(bool refresh) {
  bool drew = refresh;

  if (refresh) {
    display.clearDisplay();
    resetText();
  }

  drew |= layout.render(refresh);

  if (drew)
    display.display();

  return drew;
}
