#include "ui_setup.h"


namespace PulseWidthImages {
  const unsigned char half[] = { // 15 x 12
    0x00, 0x00, 0x00, 0x00, 0x0f, 0xe0, 0x0f, 0xe0, 0x08, 0x20, 0x08, 0x20, 0x08, 0x20, 0x08, 0x20,
    0x08, 0x20, 0x38, 0x38, 0x38, 0x38, 0x00, 0x00,
  };
  const unsigned char third[] = { // 15 x 12
    0x00, 0x00, 0x00, 0x00, 0x0f, 0x80, 0x0f, 0x80, 0x08, 0x80, 0x08, 0x80, 0x08, 0x80, 0x08, 0x80,
    0x08, 0x80, 0x38, 0xf8, 0x38, 0xf8, 0x00, 0x00,
  };
  const unsigned char fourth[] = { // 15 x 12
    0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x0f, 0x00, 0x09, 0x00, 0x09, 0x00, 0x09, 0x00, 0x09, 0x00,
    0x09, 0x00, 0x39, 0xf8, 0x39, 0xf8, 0x00, 0x00,
  };
  const unsigned char spike[] = { // 15 x 12
    0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x08, 0x00, 0x08, 0x00, 0x08, 0x00, 0x08, 0x00, 0x08, 0x00,
    0x08, 0x00, 0x3f, 0xf8, 0x37, 0xf8, 0x00, 0x00,
  };
  const unsigned char beat16[] = { // 15 x 12
    0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x00, 0x80, 0x00, 0xe0, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80,
    0x03, 0x80, 0x07, 0x80, 0x03, 0x00, 0x00, 0x00,
  };
  const unsigned char beat32[] = { // 15 x 12
    0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x00, 0x80, 0x00, 0xe0, 0x00, 0x80, 0x00, 0xe0, 0x00, 0x80,
    0x03, 0x80, 0x07, 0x80, 0x03, 0x00, 0x00, 0x00,
  };
}


PulseWidthField::PulseWidthField(
    int16_t x, int16_t y, uint16_t w, uint16_t h,
    PulseWidth& value
    )
    : ValueField(x, y, w, h, value,
        { pulseFixedShort,
          pulseDutyQuarter, pulseDutyThird, pulseDutyHalf,
          pulseDuration16, pulseDuration32
        })
    { }

void PulseWidthField::redraw() {
  const uint8_t *bitmap;

  switch (value) {

    case pulseFixedShort:   bitmap = PulseWidthImages::spike;   break;

    case pulseDutyHalf:     bitmap = PulseWidthImages::half;    break;
    case pulseDutyThird:    bitmap = PulseWidthImages::third;   break;
    case pulseDutyQuarter:  bitmap = PulseWidthImages::fourth;  break;

    case pulseDuration16:   bitmap = PulseWidthImages::beat16;  break;
    case pulseDuration32:   bitmap = PulseWidthImages::beat32;  break;

    default:
      ValueField<PulseWidth>::redraw();
      return;
  }

  display.drawBitmap(x, y, bitmap, 15, 12, foreColor());
  valueAsDrawn = value;
}
