#include "ui_memory.h"

#include <Arduino.h>

namespace {
  struct Point {
    int16_t x;
    int16_t y;
  };

  const Point spots[] =
    { { 4, 9 }, { 12, 9 }, { 12, 19 }, { 4, 19 } };
}


void MemoryField::redraw() {
  for (int16_t i = 1; i <= 4; ++i) {
    const Point& s = spots[i-1];
    const auto cx = x + s.x;
    const auto cy = y + s.y;

    if (memory == static_cast<int>(i) == isSelected()) {
      display.fillCircle(cx, cy, 3, foreColor());
    } else {
      display.drawCircle(cx, cy, 3, foreColor());
    }
  }
}

void MemoryField::update(int dir) {
  memory = constrain(memory + dir, 1, 4);
  outOfDate();
}
