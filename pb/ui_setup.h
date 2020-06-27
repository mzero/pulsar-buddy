#ifndef _INCLUDE_UI_SETUP_H_
#define _INCLUDE_UI_SETUP_H_

#include "state.h"
#include "ui_field.h"


class PulseWidthField : public ValueField<PulseWidth> {
public:
  PulseWidthField(
      int16_t x, int16_t y, uint16_t w, uint16_t h,
      PulseWidth& value
      );

protected:
  virtual void redraw();
};

#endif // _INCLUDE_UI_SETUP_H_
