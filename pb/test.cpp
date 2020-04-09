#include "test.h"

#include "controls.h"
#include "display.h"

#include <Arduino.h>
#include <wiring_private.h>


namespace {

  const uint32_t pinT = PIN_SPI_MISO;
  const uint32_t pinB = PIN_SPI_SCK;
  const uint32_t pinM = PIN_SERIAL1_TX;
  const uint32_t pinS = PIN_SPI_MOSI;
  const uint32_t pinC = PIN_A1;
  const uint32_t pinO = PIN_A2;
    // FIXME: should be dervied from the same source as the pin constants
    // in timer_hw.cpp


  struct Mode {
      const char* name;
      const char* pattern;
  };

  const Mode modes[] = {
    { "-",   "-" },
    { "T",   "T" },
    { "B",   "B" },
    { "M",   "M" },
    { "S",   "S" },
    { "*",   "*" },
    { "T~",  "T---" },
    { "B~",  "B---" },
    { "M~",  "M---" },
    { "S~",  "S---" },
    { "*~",  "TBMS" },
    { 0, 0 }
  };

  class TestState {
    bool outT, outB, outM, outS;
    bool inC, inO;

    int modeIndex;
    const Mode* mode;

    int phase;

    bool needRedraw;


    TestState() { setMode(0); }

    void draw() {
      display.clearDisplay();
      display.drawFastHLine(2, 16, 12, WHITE);

      drawSignal(inO,   8,  8, 22,  6);
      drawSignal(inC,   8, 23, 39,  6);
      drawSignal(outT, 33, 16, 22, 26);
      drawSignal(outB, 49, 16, 39, 26);
      drawSignal(outM, 66, 16, 56, 26);
      drawSignal(outS, 81, 16, 73, 26);

      display.setCursor(100, 12);
      display.setTextColor(WHITE, BLACK);
      display.print(mode->name);

      display.display();

      needRedraw = false;
    }

    static void drawSignal(bool on,
      int16_t xPin, int16_t yPin, int16_t xBanana, int16_t yBanana)
    {
      int16_t xtra = on ? 2 : 0;

      display.fillCircle(xPin, yPin, 1 + xtra, WHITE);
      display.fillCircle(xBanana, yBanana, 3 + xtra, WHITE);
      display.fillCircle(xBanana, yBanana, 2, BLACK);
    }

    void updateMode(Encoder::Update update) {
      int i = modeIndex + update.dir();
      if (i < 0 || modes[i].name == 0)
        return;
      if (i != modeIndex)
        setMode(i);
    }

    void setMode(int i) {
      modeIndex = i;
      mode = &modes[i];
      setPhase(0);

      needRedraw |= true;
    }

    void incrementPhase() {
      int p = phase + 1;
      if (mode->pattern[p] == '\0') {
        p = 0;
      }
      if (p != phase)
        setPhase(p);
    }

    void setPhase(int p) {
      phase = p;
      setOutputs(mode->pattern[phase]);
      setInputs();
    }

    static void initPins() {
      pinMode(pinT, OUTPUT); pinPeripheral(pinT, PIO_OUTPUT);
      pinMode(pinB, OUTPUT); pinPeripheral(pinB, PIO_OUTPUT);
      pinMode(pinM, OUTPUT); pinPeripheral(pinM, PIO_OUTPUT);
      pinMode(pinS, OUTPUT); pinPeripheral(pinS, PIO_OUTPUT);

      pinMode(pinC, INPUT_PULLUP);  pinPeripheral(pinC, PIO_INPUT_PULLUP);
      pinMode(pinO, INPUT_PULLUP);  pinPeripheral(pinO, PIO_INPUT_PULLUP);
    }

    void setOutputs(char op) {
      bool t = op == 'T' || op == '*';
      bool b = op == 'B' || op == '*';
      bool m = op == 'M' || op == '*';
      bool s = op == 'S' || op == '*';

      needRedraw =
        needRedraw || outT != t || outB != b || outM != m || outS != s;

      digitalWrite(pinT, (outT = t) ? LOW : HIGH);
      digitalWrite(pinB, (outB = b) ? LOW : HIGH);
      digitalWrite(pinM, (outM = m) ? LOW : HIGH);
      digitalWrite(pinS, (outS = s) ? LOW : HIGH);
        // outputs are inverted
    }

    void setInputs() {
      // simulated
      bool c = digitalRead(pinC) == LOW;
      bool o = digitalRead(pinO) == LOW;
        // inputs are inverted

      needRedraw =
        needRedraw || inC != c || inO != o;

      inO = o;
      inC = c;
    }

    const unsigned long beatPeriod = 60000 / 120 / 4;
      // sixteenth notes at 120bpm

    void loop() {
      auto nextPulse = millis() + beatPeriod;

      while (true) {

        if (millis() >= nextPulse) {
          nextPulse += beatPeriod;
          incrementPhase();
        }

        auto u = encoder.update();
        if (u.active()) {
          updateMode(u);
        }

        auto s = encoderButton.update();
        if (s == Button::Down) {
          return;
        }

        setInputs();

        if (needRedraw)
          draw();

        delay(5);
        yield();
      }
    }

  public:
    static void run() {
      TestState::initPins();

      TestState ts;
      ts.loop();
    }

  };
}



void testLoop() {
  TestState::run();
}
