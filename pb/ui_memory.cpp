#include "ui_memory.h"

#include <Arduino.h>

namespace {
  struct Point {
    int16_t x;
    int16_t y;
  };

  const Point spots[] =
    { { 4, 6 }, { 12, 6 }, { 12, 16 }, { 4, 16 } };
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

bool MemoryField::click(ButtonState s) {
  switch (s) {
    case buttonDown:
      return true;
    case buttonDownLong:
      Serial.print("memory save into ");
      Serial.println(memory);
      return true;
    case buttonUp:
      Serial.print("memory load from ");
      Serial.println(memory);
      return false;
    case buttonUpLong:
      return false;
    default:
      return false;
  }
}
void MemoryField::update(int dir) {
  memory = constrain(memory + dir, 1, 4);
  outOfDate();
}



void PendingIndicator::render(bool refresh) {
  bool pending = queryFunction();
  if (refresh || lastPending != pending) {
    lastPending = pending;

    display.fillTriangle(
        x,     y,
        x + 3, y,
        x,     y + 3,
        pending ? WHITE : BLACK);
  }
}


