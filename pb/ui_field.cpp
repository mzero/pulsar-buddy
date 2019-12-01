#include "ui_field.h"


void Field::render(bool force) {
  if (!force && !needsUpdate)
    return;

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

void Field::enter(bool alternate) { }
void Field::exit() { }

bool Field::click(ButtonState s) { return false; }
void Field::update(int dir) { }


template<>
void ValueField< uint16_t >::redraw() {
  display.setTextColor(foreColor());
  centerNumber(value, x, y, w, h);
}

