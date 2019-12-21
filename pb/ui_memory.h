#ifndef _INCLUDE_UI_MEMORY_H_
#define _INCLUDE_UI_MEMORY_H_

#include <stdint.h>

#include "ui_field.h"


class MemoryField : public Field {
public:
  MemoryField(int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t& m)
    : Field(x, y, w, h), mode(displayCurrent), memory(m)
    { }

    virtual void enter(bool);
    virtual void exit();

    virtual bool click(Button::State);
    virtual void update(Encoder::Update);

protected:
    virtual bool isOutOfDate();
    virtual void redraw();

private:
    enum Mode { displayCurrent, selectLoad, selectSave };
    uint8_t index();

    Mode mode;
    uint8_t& memory;
    uint8_t selection;

    Mode modeAsDrawn;
    uint8_t indexAsDrawn;
};


class PendingIndicator {
public:
  PendingIndicator(int16_t x, int16_t y, bool(*f)())
    : x(x), y(y), queryFunction(f), lastPending(false)
    { }

  bool render(bool refresh);

private:
  int16_t x;
  int16_t y;
  bool (*const queryFunction)();

  bool lastPending;
};

#endif // _INCLUDE_UI_MEMORY_H_
