#include "ui_field.h"


void Field::render() {
  if (!needsUpdate)
    return;

  forceRender();
}

void Field::forceRender() {
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

void Field::update(int dir) {
}



template<>
void ValueField< uint16_t >::redraw() {
  display.setTextColor(foreColor());
  centerNumber(value, x, y, w, h);
}

