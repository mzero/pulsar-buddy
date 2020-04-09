#include "test.h"

#include "controls.h"
#include "display.h"


namespace {

  struct TestState {
    bool outS, outM, outB, outT;
    bool inC, inO;
  };

  void drawSignal(bool on,
    int16_t xPin, int16_t yPin, int16_t xBanana, int16_t yBanana)
  {
    int16_t xtra = on ? 2 : 0;

    display.fillCircle(xPin, yPin, 1 + xtra, WHITE);
    display.fillCircle(xBanana, yBanana, 3 + xtra, WHITE);
    display.fillCircle(xBanana, yBanana, 2, BLACK);
  }

  void drawState(const TestState& ts) {
    display.clearDisplay();
    display.drawFastHLine(2, 16, 12, WHITE);

    drawSignal(ts.inC,   8,  8, 22,  6);
    drawSignal(ts.inO,   8, 23, 39,  6);
    drawSignal(ts.outT, 33, 16, 22, 26);
    drawSignal(ts.outB, 49, 16, 39, 26);
    drawSignal(ts.outM, 66, 16, 56, 26);
    drawSignal(ts.outS, 81, 16, 73, 26);

    display.display();
  }
}

void testLoop() {
  TestState ts = { false, true, false, false, true, false };

  while (true) {
    drawState(ts);

    auto s = encoderButton.update();
    if (s == Button::Down) {
      return;
    }

    delay(33);
    yield();
  }
}
