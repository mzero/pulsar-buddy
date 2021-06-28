#include "display.h"

#include <initializer_list>
#include <Adafruit_GFX.h>
#include <Wire.h>

#include <Fonts/FreeSerifBold9pt7b.h>
#include <Fonts/Picopixel.h>

#include "config.h"


#define FONT FreeSerifBold9pt7b
#define FONT_TINY Picopixel

#define DIGIT_WIDTH 9
#define DIGIT_HEIGHT 12

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 32

Adafruit_SSD1306 display =
  Adafruit_SSD1306(DISPLAY_WIDTH, DISPLAY_HEIGHT, &Wire, -1, 1000000UL);


// Display is mounted upside down on PCB, so "normal" is rotation 2,
// and "flipped" is rotation 0.

void setRotationNormal() {
  display.setRotation(configuration.options.flipDisplay ? 0 : 2);
}

void setRotationSideways() {
  display.setRotation(configuration.options.flipDisplay ? 3 : 1);
}


void initializeDisplay() {
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32
  display.cp437();
  setRotationNormal();
}

void resetText() {
  display.setTextSize(1);
  display.setFont(&FONT);
  display.setTextColor(WHITE);
  display.setTextWrap(false);
}

void smallText() {
  display.setFont();
}

void tinyText() {
  display.setTextSize(1);
  display.setFont(&FONT_TINY);
  display.setTextColor(WHITE);
  display.setTextWrap(false);
}

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
void centerNumber(unsigned int n,
  uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  char buf[8];
  utoa(n, buf, 10);
  centerText(buf, x, y, w, h);
}


namespace {
  const unsigned long saverStartDelay = 15 * 60 * 1000;

  unsigned long saverStartAt = 0;
  bool          saverRunning = false;

  size_t        savedSize = 0;
  uint8_t*      savedDisplay = NULL;

  unsigned long saverBumpTime;
  int16_t       saverPhase;

  const unsigned char wipePattern[] = {  // 24 x 32
    B00000010, B10101011, B11111111,
    B00000010, B10101011, B11111111,
    B00000010, B10101011, B11111111,
    B00000101, B01010111, B11111110,
    B00000101, B01010111, B11111110,
    B00000101, B01010111, B11111110,
    B00000101, B01010111, B11111110,
    B00000101, B01010111, B11111110,
    B00000101, B01010111, B11111110,
    B00001010, B10101111, B11111100,
    B00001010, B10101111, B11111100,
    B00001010, B10101111, B11111100,
    B00001010, B10101111, B11111100,
    B00001010, B10101111, B11111100,
    B00010101, B01011111, B11111000,
    B00010101, B01011111, B11111000,
    B00010101, B01011111, B11111000,
    B00010101, B01011111, B11111000,
    B00010101, B01011111, B11111000,
    B00101010, B10111111, B11110000,
    B00101010, B10111111, B11110000,
    B00101010, B10111111, B11110000,
    B00101010, B10111111, B11110000,
    B00101010, B10111111, B11110000,
    B00101010, B10111111, B11110000,
    B01010101, B01111111, B11100000,
    B01010101, B01111111, B11100000,
    B01010101, B01111111, B11100000,
    B01010101, B01111111, B11100000,
    B01010101, B01111111, B11100000,
    B10101010, B11111111, B11000000,
    B10101010, B11111111, B11000000,
  };
}

bool updateSaver(bool redrawn) {
  auto now = millis();

  if (redrawn) {
    saverStartAt = now + saverStartDelay;
    saverRunning = false;
    return false;
  }

  if (now < saverStartAt)
    return false;

  if (!saverRunning) {
    if (!savedDisplay) {
      savedSize = DISPLAY_WIDTH * ((DISPLAY_HEIGHT + 7) / 8);
      savedDisplay = (uint8_t *)malloc(savedSize);
    }
    memcpy(savedDisplay, display.getBuffer(), savedSize);
    saverRunning = true;
    saverPhase = 0;
    saverBumpTime = now - 1;
  }

  if (now > saverBumpTime) {
    display.clearDisplay();
    display.drawBitmap(saverPhase - 24, 0, wipePattern, 24, 32, WHITE);

    auto d = display.getBuffer();
    auto s = savedDisplay;
    for (auto n = savedSize; n > 0; --n)
      *d++ &= *s++;

    display.display();

    saverPhase += 2;
    if (saverPhase >= DISPLAY_WIDTH + 24) {
      saverPhase = 0;
      saverBumpTime += 2000;  // pause between swipes
    }
    saverBumpTime += 50;      // speed of swipe
  }
  return true;
}

void dumpDisplayPBM(Print& stream) {
  stream.println("");
  stream.println("P1");

  const int w = display.width();
  const int h = display.height();
  const int border = 6;
  const int jbegin = 0 - border;
  const int jend = h + border;
  const int ibegin = 0 - border;
  const int iend = w + border;

  stream.print(w + 2*border); stream.print(' '); stream.println(h + 2*border);

  for (int j = jbegin; j < jend; ++j) {
    for (int i = ibegin; i < iend; ++i) {
      bool p = (0 <= j && j < h && 0 <= i && i < w)
        ? display.getPixel(i, j)
        : BLACK;
      stream.print(p == WHITE ? " 0" : " 1");
        // 1 is black in PBM
    }
    stream.println("");
  }
  stream.println("");
}


