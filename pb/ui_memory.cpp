#include "ui_memory.h"

#include "layout.h"
#include "state.h"

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

    int index = (doLoad || doSave) ? selection : memory;
    bool active = index == static_cast<int>(i);
    if (!active && doSave)
      // don't draw other slots while saving
      continue;

    bool fill = isSelected() ? active : !active;

    if (fill) {
      display.fillCircle(cx, cy, 3, foreColor());
    } else {
      display.drawCircle(cx, cy, 3, foreColor());
    }
  }
}

void MemoryField::enter(bool alternate) {
  selection = memory;
  doLoad = !alternate;
  doSave = alternate;

  if (doLoad) {
    showMemoryPreview(selection);
    displayOutOfDate();
  }
  if (doSave) {
    outOfDate();
  }
}

void MemoryField::exit() {
  if (doLoad) {
    doLoad = false;
    endMemoryPreview();
    displayOutOfDate();
  }
  if (doSave) {
    doSave = false;
    outOfDate();
  }
}

bool MemoryField::click(ButtonState s) {
  switch (s) {
    case buttonDown:
    case buttonDownLong:
      return true;

    case buttonUp:
      if (doSave) {
        memory = selection;
        outOfDate();
        storeToMemory(selection);
      }
      if (doLoad) {
        endMemoryPreview();
        memory = selection;
        loadFromMemory(selection);
        displayOutOfDate();
      }
      return false;

    case buttonUpLong:
      return false;

    default:
      return false;
  }
}
void MemoryField::update(int dir) {
  selection = constrain(selection + dir, 1, 4);
  outOfDate();

  if (doLoad) {
    showMemoryPreview(selection);
    displayOutOfDate();
  }
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


