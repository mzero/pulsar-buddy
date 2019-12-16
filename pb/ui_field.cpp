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
void Field::update(Encoder::Update update) { }

bool Field::isOutOfDate() { return selectedAsDrawn != selected; }

