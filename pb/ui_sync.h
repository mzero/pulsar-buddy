#ifndef _INCLUDE_UI_SYNC_H_
#define _INCLUDE_UI_SYNC_H_

#include "state.h"
#include "timing.h"
#include "ui_field.h"


class SyncField : public Field {
public:
  SyncField(int16_t x, int16_t y, uint16_t w, uint16_t h, State& state)
    : Field(x, y, w, h), state(state)
    { }

    virtual void enter(bool);
    virtual void exit();

    virtual void update(Encoder::Update);

protected:
    virtual bool isOutOfDate();
    virtual void redraw();

private:
    enum Mode { displayBPM, displaySync };

    State& state;
    Mode mode;

    Mode modeAsDrawn;
    bpm_t bpmAsDrawn;
    SyncMode pendingSync;
    SyncMode syncAsDrawn;
};


#endif // _INCLUDE_UI_SYNC_H_
