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

uint8_t MemoryField::index() {
  return (mode == displayCurrent) ? memory : selection;
}

bool MemoryField::isOutOfDate() {
  return
    modeAsDrawn != mode
    || indexAsDrawn != index()
    || Field::isOutOfDate();
}

void MemoryField::redraw() {
  uint8_t i = index();

  for (uint8_t j = 1; j <= 4; ++j) {
    const Point& s = spots[j-1];
    const auto cx = x + s.x;
    const auto cy = y + s.y;

    bool active = i == j;
    if (!active && mode == selectSave)
      // don't draw other slots while saving
      continue;

    bool fill = isSelected() ? active : !active;

    if (fill) {
      display.fillCircle(cx, cy, 3, foreColor());
    } else {
      display.drawCircle(cx, cy, 3, foreColor());
    }
  }

  modeAsDrawn = mode;
  indexAsDrawn = i;
}

void MemoryField::enter(bool alternate) {
  mode = alternate ? selectSave : selectLoad;
  selection = memory;

  if (mode == selectLoad)
    showMemoryPreview(selection);
}

void MemoryField::exit() {
  if (mode == selectLoad)
    endMemoryPreview();

  mode = displayCurrent;
}

bool MemoryField::click(Button::State s) {
  switch (s) {
    case Button::Down:
    case Button::DownLong:
      return true;

    case Button::Up:
      switch (mode) {
        case selectLoad:
          endMemoryPreview();
          loadFromMemory(selection);
          mode = displayCurrent;
          memory = selection;
          break;

        case selectSave:
          storeToMemory(selection);
          mode = displayCurrent;
          memory = selection;
          break;

        default:
          break;
      }
      return false;

    case Button::UpLong:
      return false;

    default:
      return false;
  }
}

void MemoryField::update(Encoder::Update update) {
  selection = constrain(selection + update.dir(), 1, 4);

  if (mode == selectLoad)
    showMemoryPreview(selection);
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


