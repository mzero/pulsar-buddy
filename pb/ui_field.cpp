#include "ui_field.h"


void Field::render(bool force) {
  if (!force && !isOutOfDate())
    return;

  display.fillRect(x, y, w, h, backColor());
  redraw();
  selectedAsDrawn = selected;
}

void Field::select(bool s) {
  selected = s;
}

void Field::enter(bool alternate) { }
void Field::exit() { }

bool Field::click(ButtonState s) { return false; }
void Field::update(int dir) { }

bool Field::isOutOfDate() { return selectedAsDrawn != selected; }

template<>
void ValueField< uint16_t >::redraw() {
  display.setTextColor(foreColor());
  centerNumber(value, x, y, w, h);
  valueAsDrawn = value;
}

