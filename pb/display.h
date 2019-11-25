#ifndef _INCLUDE_DISPLAY_H_
#define _INCLUDE_DISPLAY_H_

#include <Adafruit_SSD1306.h>

#define FONT FreeSerifBold9pt7b
#define DIGIT_WIDTH 9
#define DIGIT_HEIGHT 12

#define WHITE SSD1306_WHITE
#define BLACK SSD1306_BLACK


extern Adafruit_SSD1306 display;

void resetText();
void centerText(const char* s, int16_t x, int16_t y, uint16_t w, uint16_t h);
void centerNumber(uint16_t n, uint16_t x, uint16_t y, uint16_t w, uint16_t h);


#endif // _INCLUDE_DISPLAY_H_
