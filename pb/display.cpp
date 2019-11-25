#include "display.h"

#include <Adafruit_GFX.h>
#include <Wire.h>

#include <Fonts/FreeSerifBold9pt7b.h>

#define FONT FreeSerifBold9pt7b
#define DIGIT_WIDTH 9
#define DIGIT_HEIGHT 12

Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire);

void resetText() {
  display.setTextSize(1);
  display.setFont(&FONT);
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
void centerNumber(uint16_t n, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  char buf[8];
  utoa(n, buf, 10);
  centerText(buf, x, y, w, h);
}
