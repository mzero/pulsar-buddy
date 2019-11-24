#include <algorithm>
#include <initializer_list>

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
  Field(int16_t x, int16_t y, uint16_t w, uint16_t h)
    : x(x), y(y), w(w), h(h), selected(false), needsUpdate(true)
    { };

  void render();
  void forceRender();

  void select(bool);
  inline void select() { select(true); };
  inline void deselect() { select(false); };

  virtual void update(int);

protected:
  inline void outOfDate() { needsUpdate = true; };
  inline uint16_t foreColor() { return selected ? BLACK : WHITE; };
  inline uint16_t backColor() { return selected ? WHITE : BLACK; };

  virtual void redraw() = 0;

  const     int16_t   x, y;
  const     uint16_t  w, h;

private:
  bool      selected;
  bool      needsUpdate;
};

void Field::render() {
  if (!needsUpdate)
    return;

  forceRender();
}

void Field::forceRender() {
  display.fillRect(x, y, w, h, backColor());
  redraw();
  needsUpdate = false;
}

void Field::select(bool s) {
  if (selected != s) {
    selected = s;
    needsUpdate = true;
  }
}

void Field::update(int dir) {
}



template< typename T >
class ValueField : public Field {
public:
  ValueField(
      int16_t x, int16_t y, uint16_t w, uint16_t h,
      T& value, std::initializer_list<T> options
      )
    : Field(x, y, w, h), value(value),
      options({}),
      numOptions(min(maxOptions, (int)(options.size())))
    {
      std::copy(
        options.begin(), options.begin() + numOptions,
        const_cast<T*>(this->options));
    };

  virtual void update(int dir);

protected:
  virtual void redraw();

  T&        value;

  static const int maxOptions = 16;
  const T          options[maxOptions];
  const int        numOptions;
};


template< typename T >
void ValueField<T>::update(int dir) {
  int i;
  for (i = 0; i < numOptions; i++) {
    if (value == options[i]) break;
  }
  value = options[constrain(i + dir, 0, numOptions - 1)];
  outOfDate();
}

template<>
void ValueField< uint16_t >::redraw() {
  display.setTextColor(foreColor());
  centerNumber(value, x, y, w, h);
}



class BeatField : public ValueField<uint16_t> {
public:
  BeatField(
      int16_t x, int16_t y, uint16_t w, uint16_t h,
      uint16_t& value
      )
    : ValueField(x, y, w, h, value, {16, 8, 4, 2})
    { };

protected:
  virtual void redraw();

private:
};

// 'note2', 12x28px
const unsigned char imageNote2 [] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x80, 0x33, 0xc0, 0x71, 0xe0, 0x79, 0xe0, 0x78, 0xe0,
  0x3c, 0xc0, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
// 'note4', 12x28px
const unsigned char imageNote4 [] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00,
  0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00,
  0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x1e, 0x00, 0x3e, 0x00, 0x7e, 0x00, 0x7e, 0x00,
  0x7c, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00
};
// 'note8', 12x28px
const unsigned char imageNote8 [] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x00, 0x03, 0x00, 0x03, 0x00, 0x03, 0x80, 0x03, 0xc0,
  0x02, 0xe0, 0x02, 0x60, 0x02, 0x20, 0x02, 0x20, 0x02, 0x20, 0x02, 0x60, 0x02, 0x40, 0x02, 0x80,
  0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x1e, 0x00, 0x3e, 0x00, 0x7e, 0x00, 0x7e, 0x00,
  0x7c, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00
};
// 'note16', 12x28px
const unsigned char imageNote16 [] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x00, 0x03, 0x00, 0x03, 0x00, 0x03, 0x80, 0x03, 0xc0,
  0x02, 0xe0, 0x03, 0x60, 0x03, 0x20, 0x03, 0xa0, 0x03, 0xe0, 0x02, 0xe0, 0x02, 0x60, 0x02, 0x20,
  0x02, 0x20, 0x02, 0x60, 0x02, 0x40, 0x02, 0x80, 0x1e, 0x00, 0x3e, 0x00, 0x7e, 0x00, 0x7e, 0x00,
  0x7c, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00
};

void BeatField::redraw() {
  const uint8_t *bitmap;

  switch (value) {
    case 16:    bitmap = imageNote16;   break;
    case  8:    bitmap = imageNote8;    break;
    case  4:    bitmap = imageNote4;    break;
    case  2:    bitmap = imageNote2;    break;
    default:
      ValueField<uint16_t>::redraw();
      return;
  }

  display.drawBitmap(x, y, bitmap, 12, 28, foreColor());
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


auto fieldNumberMeasures
  = ValueField<uint16_t>(18,  0, 10, 32,
      settings.numberMeasures,
      { 1, 2, 3, 4, 5, 6, 7, 8 }
      );

auto fieldBeatsPerMeasure
  = ValueField<uint16_t>(38,  0, 24, 14,
      settings.beatsPerMeasure,
      { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 }
      );

auto fieldBeatUnit
  = ValueField<uint16_t>(38, 18, 24, 14,
      settings.beatUnit,
      { 1, 2, 4, 8, 16 }
      );

auto fieldTupletCount
  = ValueField<uint16_t>(66,  0, 10, 32,
      settings.tupletCount,
      { 2, 3, 4, 5, 6, 7, 8, 9 }
      );
auto fieldTupletTime
  = ValueField<uint16_t>(82,  0, 10, 32,
      settings.tupletTime,
      { 2, 3, 4, 6, 8 }
      );
auto fieldTupletUnit
  = BeatField(94, 2, 12, 28,
      settings.tupletUnit
      );

Field *const selectableFields[] =
  { &fieldNumberMeasures,
    &fieldBeatsPerMeasure,
    &fieldBeatUnit,
    &fieldTupletCount,
    &fieldTupletTime,
    &fieldTupletUnit
  };

const int numberOfSelectableFields = 6;
  // sizeof(selectableFields) / sizeof(selectableFields[0]);

enum SelectMode { selectNone, selectField, selectValue };

SelectMode selectMode = selectNone;
int selectedFieldIndex = 0;

void resetSelection() {
  selectMode = selectNone;
  selectableFields[selectedFieldIndex]->deselect();
}

void updateSelection(int dir) {
  switch (selectMode) {

    case selectNone:
      selectMode = selectField;
      selectableFields[selectedFieldIndex]->select();
      break;

    case selectField:
      selectableFields[selectedFieldIndex]->deselect();
      selectedFieldIndex =
        constrain(selectedFieldIndex + dir, 0, numberOfSelectableFields - 1);
      selectableFields[selectedFieldIndex]->select();
      break;

    case selectValue:
      selectableFields[selectedFieldIndex]->update(dir);
      break;
  }
}

void clickSelection() {
  switch (selectMode) {

    case selectNone:
      selectMode = selectField;
      selectableFields[selectedFieldIndex]->select();
      break;

    case selectField:
      selectMode = selectValue;
      break;

    case selectValue:
      selectMode = selectField;
      break;
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
  //    the time signature dividing line
  display.drawLine(38, 16, 62, 16, WHITE);

  drawSeparator(64);

  // Tuplet area
  //    the :
  centerText(":", 77, 0, 4, 32);

  drawSeparator(108);

  // Memory area
}

void drawAll(bool refresh) {
  display.dim(false);

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
      } else if (quads < -1) {
        r = -1;
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
  IdleTimeout(unsigned long period);
  void activity();
  bool update();      // returns true on start of idle

private:
  const unsigned long period;
  bool idle;
  unsigned long idleAtTime;
};

IdleTimeout::IdleTimeout(unsigned long period)
  : idle(true), period(period)
  { }

void IdleTimeout::activity() {
  idle = false;
  idleAtTime = millis() + period;
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

IdleTimeout selectionTimeout(2000);
IdleTimeout dimTimeout(5000);
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
  selectionTimeout.activity();
  dimTimeout.activity();
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

  if (selectionTimeout.update()) {
    resetSelection();
    drawAll(false);
    return;
  }

  if (dimTimeout.update()) {
    display.dim(true);
    return;
  }
}
