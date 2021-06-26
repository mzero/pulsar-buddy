#include "layout.h"

#include "display.h"
#include "state.h"
#include "ui_field.h"
#include "ui_layout.h"
#include "ui_memory.h"
#include "ui_music.h"
#include "ui_setup.h"
#include "ui_sync.h"


namespace {
  // NOTE: Be careful when adjusting the range of available
  // settings, because they can produce timing periods that
  // are too big for the timers.

  auto fieldBpm
    = BpmField(0, 0, 15, 32, userState());

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


  const std::initializer_list<Field*> mainFields =
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
    MainPage() : Layout(mainFields, 1) { }
    bool render(bool refresh);
  protected:
    void redraw();
  };

  bool MainPage::render(bool refresh) {
    bool updated = Layout::render(refresh);
    updated |= pendingLoopIndicator.render(refresh);
    updated |= pendingTupletIndicator.render(refresh);
    updated |= pendingMemoryIndicator.render(refresh);
    return updated;
  }


  auto fieldReturnToMain
    = ReturnField(0, 5, 15, 22);


  auto fieldSync
    = SyncField(19, 0, 15, 32, userState());

  auto fieldOtherSync
    = OtherSyncField(111, 0, 15, 32, userState());

  constexpr int16_t x_pins = 37;
  constexpr int16_t x_pin_width = 16;
  constexpr int16_t x_pinT = x_pins + 0 * x_pin_width;
  constexpr int16_t x_pinB = x_pins + 1 * x_pin_width;
  constexpr int16_t x_pinM = x_pins + 2 * x_pin_width;
  constexpr int16_t x_pinS = x_pins + 3 * x_pin_width;

  auto pulseWitdhT = PulseWidthField(x_pinT + 4, 19, 15, 12, userState().pulseWidthT);
  auto pulseWitdhB = PulseWidthField(x_pinB + 4, 19, 15, 12, userState().pulseWidthB);
  auto pulseWitdhM = PulseWidthField(x_pinM + 4, 19, 15, 12, userState().pulseWidthM);
  auto pulseWitdhS = PulseWidthField(x_pinS + 4, 19, 15, 12, userState().pulseWidthS);

  const std::initializer_list<Field*> setupFields =
    { &fieldReturnToMain,
      &fieldSync,
      &pulseWitdhT,
      &pulseWitdhB,
      &pulseWitdhM,
      &pulseWitdhS,
      &fieldOtherSync
    };


  class SetupPage : public Layout {
  public:
    SetupPage() : Layout(setupFields) { }
  protected:
    void redraw();
  };


  MainPage mainPage;
  SetupPage setupPage;


  Frame screen(mainPage);
}

void resetSelection() {
  screen.exit();
}

void updateSelection(Encoder::Update update) {
  screen.update(update);
}

void clickSelection(Button::State s) {
  screen.click(s);
}

void showMain() { screen.show(mainPage); }
void showSetup() { screen.show(setupPage); }


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

  void SetupPage::redraw() {
    drawSeparator(16);
    drawSeparator(36);
    drawSeparator(109);

    smallText();
    display.setTextColor(WHITE, BLACK);

    display.setCursor(x_pinT + 9, 2);    display.print('T');
    display.setCursor(x_pinB + 9, 2);    display.print('B');
    display.setCursor(x_pinM + 9, 2);    display.print('M');
    display.setCursor(x_pinS + 9, 2);    display.print('S');

    resetText();

    display.fillCircle(x_pinT + 11, 14, 2, WHITE);
    display.fillCircle(x_pinB + 11, 14, 2, WHITE);
    display.fillCircle(x_pinM + 11, 14, 2, WHITE);
    display.fillCircle(x_pinS + 11, 14, 2, WHITE);

  }
}


bool drawAll(bool refresh) {
  bool drew = refresh;

  if (refresh) {
    display.clearDisplay();
    resetText();
  }

  drew |= screen.render(refresh);

  if (drew)
    display.display();

  return drew;
}
