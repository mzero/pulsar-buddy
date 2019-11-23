#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSerifBold9pt7b.h>

#define FONT FreeSerifBold9pt7b
#define DIGIT_WIDTH 9
#define DIGIT_HEIGHT 12

Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire);

// OLED FeatherWing buttons map to different pins depending on board:
#if defined(ESP8266)
  #define BUTTON_A  0
  #define BUTTON_B 16
  #define BUTTON_C  2
#elif defined(ESP32)
  #define BUTTON_A 15
  #define BUTTON_B 32
  #define BUTTON_C 14
#elif defined(ARDUINO_STM32_FEATHER)
  #define BUTTON_A PA15
  #define BUTTON_B PC7
  #define BUTTON_C PC5
#elif defined(TEENSYDUINO)
  #define BUTTON_A  4
  #define BUTTON_B  3
  #define BUTTON_C  8
#elif defined(ARDUINO_FEATHER52832)
  #define BUTTON_A 31
  #define BUTTON_B 30
  #define BUTTON_C 27
#else // 32u4, M0, M4, nrf52840 and 328p
  #define BUTTON_A  9
  #define BUTTON_B  6
  #define BUTTON_C  5
#endif

#define ENCODER_SW  10
#define ENCODER_A   11
#define ENCODER_B   12

#define WHITE SSD1306_WHITE
#define BLACK SSD1306_BLACK


uint16_t bpm = 128;

void centerText(const char* s, int16_t x, int16_t y, uint16_t w, uint16_t h) {
  int16_t bx, by;
  uint16_t bw, bh;

  display.getTextBounds(s, x, y, &bx, &by, &bw, &bh);
  display.setCursor(
    x + (x - bx) + (w - bw) / 2,
    y + (y - by) + (h - bh) / 2
  );
  display.print(s);
}
void centerNumber(uint16_t n, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  char buf[8];
  utoa(n, buf, 10);
  centerText(buf, x, y, w, h);
}



class Field {
public:
  Field(uint16_t& value, int16_t x, int16_t y, uint16_t w, uint16_t h)
    : value(value), x(x), y(y), w(w), h(h), selected(false), lastDrawn(false)
    { };

  uint16_t& value;

  int16_t   x, y;
  uint16_t  w, h;

  bool      selected;

  void render();

private:
  bool      lastDrawn;
  uint16_t  lastValue;
  bool      lastSelected;
};

void Field::render() {
  if (lastDrawn && value == lastValue && selected == lastSelected)
    return;

  uint16_t foreColor = selected ? BLACK : WHITE;
  uint16_t backColor = selected ? WHITE : BLACK;

  display.fillRect(x, y, w, h, backColor);
  display.setTextColor(foreColor);
  centerNumber(value, x, y, w, h);

  lastDrawn = true;
  lastValue = value;
  lastSelected = selected;
}

struct Settings {
  uint16_t    numberMeasures;
  uint16_t    beatsPerMeasure;
  uint16_t    beatUnit;

  uint16_t    tupletCount;
  uint16_t    tupletTime;
  uint16_t    tupletUnit;
};


Settings settings = { 4, 16, 16, 3, 2, 4 };


Field fieldNumberMeasures   = Field(settings.numberMeasures,    18,  0, 10, 32);
Field fieldBeatsPerMeasure  = Field(settings.beatsPerMeasure,   38,  0, 24, 14);
Field fieldBeatUnit         = Field(settings.beatUnit,          38, 18, 24, 14);

Field fieldTupletCount      = Field(settings.tupletCount,       66,  0, 10, 32);
Field fieldTupletTime       = Field(settings.tupletTime,        82,  0, 10, 32);


Field *const selectableFields[] =
  { &fieldNumberMeasures,
    &fieldBeatsPerMeasure,
    &fieldBeatUnit,
    &fieldTupletCount,
    &fieldTupletTime,
  };
const int numberOfSelectableFields = 5;
  // sizeof(selectableFields) / sizeof(selectableFields[0]);

int selectedFieldIndex = -1;
bool selectingFields = true;

void resetSelection() {
  if (selectedFieldIndex >= 0) {
    selectableFields[selectedFieldIndex]->selected = false;
  }
  selectedFieldIndex = -1;
  selectingFields = true;
}

void selectNextField(int dir) {
  if (selectedFieldIndex >= 0) {
    selectableFields[selectedFieldIndex]->selected = false;
  }
  selectedFieldIndex =
    constrain(selectedFieldIndex + dir, 0, numberOfSelectableFields - 1);
  selectableFields[selectedFieldIndex]->selected = true;
}

void bumpSelectedFieldValue(int dir) {
  if (selectableFields[selectedFieldIndex]) {
    selectableFields[selectedFieldIndex]->value += dir;
  }
}

void updateSelection(int dir) {
  if (selectingFields) {
    selectNextField(dir);
  } else {
    bumpSelectedFieldValue(dir);
  }
}

void clickSelection() {
  if (selectingFields) {
    selectingFields = false;
  } else {
    selectingFields = true;
  }
}


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
  fieldBeatUnit.render();
}

void drawTuplet() {
  fieldTupletCount.render();
  fieldTupletTime.render();
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
  //    the time signature dividing line
  display.drawLine(38, 16, 62, 16, WHITE);

  drawSeparator(64);

  // Tuplet area
  //    the :
  centerText(":", 77, 0, 4, 32);
  //    the 1/4 note (for now)
  display.fillCircle(97, 24, 2, WHITE);
  display.fillCircle(98, 23, 2, WHITE);
  display.drawLine(100, 6, 100, 21, WHITE);

  drawSeparator(108);

  // Memory area
}

void drawAll(bool refresh) {
  unsigned long t0 = millis();

  if (refresh) {
    display.clearDisplay();

    display.setTextSize(1);
    display.setFont(&FONT);
    display.setTextColor(WHITE);
    display.setTextWrap(false);

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

class Encoder {
public:
  Encoder(uint32_t pinA, uint32_t pinB);
  int update();   // reports -1 for CCW, 0 for no motion, and 1 for CW

private:
  const uint32_t pinA;
  const uint32_t pinB;

  int a;
  int b;

  int quads;
};

Encoder::Encoder(uint32_t pinA, uint32_t pinB)
    : pinA(pinA), pinB(pinB)
{
  pinMode(pinA, INPUT_PULLUP);
  pinMode(pinB, INPUT_PULLUP);
  a = digitalRead(pinA);
  b = digitalRead(pinB);
  quads = 0;
}

int Encoder::update() {
  int newA = digitalRead(pinA);
  int newB = digitalRead(pinB);

  int r = 0;

  if (newA != a || newB != b) {
    if (newA == a) {
      quads += (newA == newB) ? 1 : -1;
    } else if (newB == b) {
      quads += (newA != newB) ? 1 : -1;
    }

    a = newA;
    b = newB;

    if (a && b) {
      if (quads > 1) {
        r = 1;
        Serial.println("Encoder CW");
      } else if (quads < -1) {
        r = -1;
        Serial.println("Encoder CWW");
      }
      quads = 0;
    }
  }

  return r;
}

class Button {
public:
    Button(uint32_t pin);
    bool update();    // reports true on press

private:
    const uint32_t pin;

    int lastState;
    int lastRead;
    unsigned long validAtTime;
};

Button::Button(uint32_t pin)
  : pin(pin)
{
  pinMode(pin, INPUT_PULLUP);   // 1 is off, 0 is pressed
  lastState = 1;
  lastRead = -1;        // will cause first update to always set it
  validAtTime = 0;
}

bool Button::update()
{
  int read = digitalRead(pin);
  if (read != lastRead) {
    lastRead = read;
    validAtTime = millis() + 50;
    return false;
  }
  if (lastRead == lastState) {
    return false;
  }
  uint32_t now = millis();
  if (now >= validAtTime) {
    lastState = lastRead;
    return lastState == 0;
  }
  return false;
}

class IdleTimeout {
public:
  IdleTimeout();
  void activity();
  bool update();      // returns true on start of idle

private:
  bool idle;
  unsigned long idleAtTime;
};

IdleTimeout::IdleTimeout()
  : idle(true)
  { }

void IdleTimeout::activity() {
  idle = false;
  idleAtTime = millis() + 2000;
}

bool IdleTimeout::update() {
  if (idle)
    return false;

  if (millis() > idleAtTime) {
    idle = true;
    return true;
  }

  return false;
}

IdleTimeout idleTimeout;
Encoder encoder(ENCODER_A, ENCODER_B);
Button encoderButton(ENCODER_SW);
Button oledButtonA(BUTTON_A);
Button oledButtonB(BUTTON_B);
Button oledButtonC(BUTTON_C);

void setup() {
  Serial.begin(9600);
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32
  drawAll(true);
}

void postAction() {
  drawAll(false);
  idleTimeout.activity();
}

void loop() {
  // delay(10);
  // yield();

  int dir = encoder.update();
  if (dir) {
    updateSelection(dir);
    postAction();
    return;
  }

  if (encoderButton.update()) {
    clickSelection();
    postAction();
    return;
  }

  if (oledButtonA.update()) {
    clickSelection();
    postAction();
    return;
  }

  if (oledButtonB.update()) {
    resetSelection();
    postAction();
    return;
  }

  if (idleTimeout.update()) {
    resetSelection();
    drawAll(false);
    return;
  }
}
