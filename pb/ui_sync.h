#ifndef _INCLUDE_UI_SYNC_H_
#define _INCLUDE_UI_SYNC_H_

#include "clock.h"
#include "state.h"
#include "timing.h"
#include "ui_field.h"


class BpmField : public Field {
public:
  BpmField(int16_t x, int16_t y, uint16_t w, uint16_t h, State& state)
    : Field(x, y, w, h), state(state)
    { }

    virtual void enter(bool);
    virtual void update(Encoder::Update);

protected:
    virtual bool isOutOfDate();
    virtual void redraw();

private:
    State& state;

    ClockStatus clockAsDrawn;
};


class SyncField : public Field {
public:
  SyncField(int16_t x, int16_t y, uint16_t w, uint16_t h, State& state)
    : Field(x, y, w, h), state(state), pending(false)
    { }

    virtual void enter(bool);
    virtual void exit();

    virtual void update(Encoder::Update);

protected:
    virtual bool isOutOfDate();
    virtual void redraw();

private:
    State& state;

    bool pending;
    SyncMode pendingSync;
    SyncMode syncAsDrawn;
};


class OtherSyncField : public Field {
public:
  OtherSyncField(int16_t x, int16_t y, uint16_t w, uint16_t h, State& state)
    : Field(x, y, w, h), state(state), pending(false)
    { }

    virtual void enter(bool);
    virtual void exit();

    virtual void update(Encoder::Update);

protected:
    virtual bool isOutOfDate();
    virtual void redraw();

private:
    State& state;

    bool pending;
    OtherSyncMode pendingSync;
    OtherSyncMode syncAsDrawn;
};



class ReturnField : public Field {
public:
  ReturnField(int16_t x, int16_t y, uint16_t w, uint16_t h)
    : Field(x, y, w, h)
    { }

    virtual void enter(bool);

protected:
    virtual void redraw();
};

#endif // _INCLUDE_UI_SYNC_H_
