#ifndef _INCLUDE_UI_MEMORY_H_
#define _INCLUDE_UI_MEMORY_H_

#include "ui_field.h"


class MemoryField : public Field {
public:
  MemoryField(int16_t x, int16_t y, uint16_t w, uint16_t h, int& m)
    : Field(x, y, w, h), memory(m)
    { }

    virtual void update(int);

protected:
    virtual void redraw();

private:

    int& memory;
};


#endif // _INCLUDE_UI_MEMORY_H_
