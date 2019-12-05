#ifndef _INCLUDE_DISPLAY_H_
#define _INCLUDE_DISPLAY_H_

#include <Adafruit_SSD1306.h>

#define WHITE SSD1306_WHITE
#define BLACK SSD1306_BLACK


extern Adafruit_SSD1306 display;

void resetText();
void smallText();
void centerText(const char* s, int16_t x, int16_t y, uint16_t w, uint16_t h);

void centerNumber(unsigned int n,
  uint16_t x, uint16_t y, uint16_t w, uint16_t h);

template< typename N >
void centerNumber(const N& n, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
  { centerNumber(static_cast<unsigned int>(n), x, y, w, h); }


#endif // _INCLUDE_DISPLAY_H_
