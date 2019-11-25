#ifndef _INCLUDE_UI_MUSIC_H_
#define _INCLUDE_UI_MUSIC_H_

#include "ui_field.h"


class TimeSignatureField : public PairField<uint16_t, uint16_t> {
public:
  TimeSignatureField(
    int16_t x, int16_t y, uint16_t w, uint16_t h,
    ValueField<uint16_t>& numberField,
    ValueField<uint16_t>& beatField
    );

protected:
    virtual void redraw();
};


class TupletRatioField : public PairField<uint16_t, uint16_t> {
public:
  TupletRatioField(
    int16_t x, int16_t y, uint16_t w, uint16_t h,
    ValueField<uint16_t>& countField,
    ValueField<uint16_t>& timeField
    );

protected:
    virtual void redraw();
};


class BeatField : public ValueField<uint16_t> {
public:
  BeatField(
      int16_t x, int16_t y, uint16_t w, uint16_t h,
      uint16_t& value
      );

protected:
  virtual void redraw();
};


#endif // _INCLUDE_UI_MUSIC_H_
