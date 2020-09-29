#include "ui_sync.h"

#include "display.h"
#include "layout.h"
#include "timer_hw.h"


namespace SyncImages {
  const unsigned char paused[] = { // 15 x 32
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x2a, 0xa8, 0x00, 0x00, 0x20, 0x08, 0x06, 0xc0, 0x26, 0xc8, 0x06, 0xc0,
    0x26, 0xc8, 0x06, 0xc0, 0x20, 0x08, 0x00, 0x00, 0x2a, 0xa8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  const unsigned char din24[] = { // 15 x 32
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x80, 0x0f, 0xe0, 0x1f, 0xf0,
    0x1f, 0xf0, 0x3f, 0xf8, 0x37, 0xd8, 0x3f, 0xf8, 0x1b, 0xb0, 0x1e, 0xf0, 0x0f, 0xe0, 0x03, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x1c, 0x18, 0x3e, 0x38, 0x46, 0x38, 0x06, 0x58, 0x0c, 0x98, 0x18, 0xfc,
    0x30, 0xfc, 0x7e, 0x18, 0x7e, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  const unsigned char din48[] = { // 15 x 32
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x80, 0x0f, 0xe0, 0x1f, 0xf0,
    0x1f, 0xf0, 0x3f, 0xf8, 0x37, 0xd8, 0x3f, 0xf8, 0x1b, 0xb0, 0x1e, 0xf0, 0x0f, 0xe0, 0x03, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x0c, 0x70, 0x1c, 0xd8, 0x1c, 0xd8, 0x2c, 0x70, 0x4c, 0xb8, 0x7e, 0xd8,
    0x7e, 0xd8, 0x0c, 0xd8, 0x0c, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  const unsigned char dinmidi[] = { // 15 x 32
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x80, 0x0f, 0xe0, 0x1f, 0xf0,
    0x1f, 0xf0, 0x3f, 0xf8, 0x37, 0xd8, 0x3f, 0xf8, 0x1b, 0xb0, 0x1e, 0xf0, 0x0f, 0xe0, 0x03, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x18, 0x30, 0x1c, 0x70, 0x1e, 0xf0, 0x1f, 0xf0, 0x1b, 0xb0, 0x19, 0x30,
    0x18, 0x30, 0x18, 0x30, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  const unsigned char dinbad[] = { // 15 x 32
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x80, 0x0f, 0xe0, 0x1f, 0xf0,
    0x1f, 0xf0, 0x3f, 0xf8, 0x37, 0xd8, 0x3f, 0xf8, 0x1b, 0xb0, 0x1e, 0xf0, 0x0f, 0xe0, 0x03, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x07, 0xc0, 0x0f, 0xe0, 0x0c, 0x60, 0x08, 0x60, 0x00, 0xe0, 0x01, 0xc0,
    0x03, 0x80, 0x03, 0x80, 0x00, 0x00, 0x03, 0x80, 0x03, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  const unsigned char usbmidi[] = { // 15 x 32
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x80, 0xff, 0xfc, 0xe7, 0x84,
    0xdf, 0xa4, 0x91, 0x84, 0xef, 0xa4, 0xf3, 0x84, 0xff, 0xfc, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x18, 0x30, 0x1c, 0x70, 0x1e, 0xf0, 0x1f, 0xf0, 0x1b, 0xb0, 0x19, 0x30,
    0x18, 0x30, 0x18, 0x30, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  const unsigned char clk32[] = { // 15 x 32
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x3f, 0x00, 0x21, 0x00,
    0x21, 0x00, 0x21, 0x08, 0x01, 0x08, 0x01, 0x08, 0x09, 0xf8, 0x19, 0xf8, 0x38, 0x00, 0x18, 0x00,
    0x18, 0x00, 0x19, 0x00, 0x1a, 0x00, 0x04, 0x00, 0x08, 0x00, 0x10, 0x00, 0x2e, 0x38, 0x13, 0x4c,
    0x03, 0x0c, 0x06, 0x18, 0x03, 0x30, 0x13, 0x64, 0x0e, 0x7c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  const unsigned char clk16[] = { // 15 x 32
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x3f, 0x00, 0x21, 0x00,
    0x21, 0x00, 0x21, 0x08, 0x01, 0x08, 0x01, 0x08, 0x09, 0xf8, 0x19, 0xf8, 0x38, 0x00, 0x18, 0x00,
    0x18, 0x00, 0x19, 0x00, 0x1a, 0x00, 0x04, 0x00, 0x08, 0x00, 0x12, 0x08, 0x26, 0x30, 0x0e, 0x60,
    0x06, 0xf0, 0x06, 0xd8, 0x06, 0xd8, 0x06, 0xd8, 0x06, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  const unsigned char clk8[] = { // 15 x 32
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x3f, 0x00, 0x21, 0x00,
    0x21, 0x00, 0x21, 0x08, 0x01, 0x08, 0x01, 0x08, 0x09, 0xf8, 0x19, 0xf8, 0x38, 0x00, 0x18, 0x00,
    0x18, 0x00, 0x19, 0x00, 0x1a, 0x00, 0x04, 0x00, 0x08, 0x00, 0x13, 0xc0, 0x26, 0x60, 0x07, 0x60,
    0x07, 0xc0, 0x03, 0xe0, 0x06, 0xe0, 0x06, 0x60, 0x03, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  const unsigned char clk4[] = { // 15 x 32
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x3f, 0x00, 0x21, 0x00,
    0x21, 0x00, 0x21, 0x08, 0x01, 0x08, 0x01, 0x08, 0x09, 0xf8, 0x19, 0xf8, 0x38, 0x00, 0x18, 0x00,
    0x18, 0x00, 0x19, 0x00, 0x1a, 0x00, 0x04, 0x00, 0x08, 0x40, 0x10, 0xc0, 0x21, 0xc0, 0x02, 0xc0,
    0x04, 0xc0, 0x07, 0xe0, 0x07, 0xe0, 0x00, 0xc0, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  const unsigned char clkbad[] = { // 15 x 32
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x3f, 0x00, 0x21, 0x00,
    0x21, 0x00, 0x21, 0x08, 0x01, 0x08, 0x01, 0x08, 0x01, 0xf8, 0x01, 0xf8, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x07, 0xc0, 0x0f, 0xe0, 0x0c, 0x60, 0x08, 0x60, 0x00, 0xe0, 0x01, 0xc0, 0x03, 0x80,
    0x03, 0x80, 0x00, 0x00, 0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  const unsigned char internal[] = { // 15 x 32
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x03, 0x80, 0x02, 0x84, 0x02, 0x98, 0x05, 0x58, 0x04, 0x60,
    0x05, 0x40, 0x08, 0xa0, 0x09, 0x20, 0x08, 0x20, 0x1d, 0x70, 0x1f, 0xf0, 0x1f, 0xf0, 0x1f, 0xf0,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
}



/**
 **  BPM Field
 **/

bool BpmField::isOutOfDate() {
  auto status = ClockStatus::current();

  return
    (clockAsDrawn != status)
    || Field::isOutOfDate();
}

void BpmField::enter(bool alternate) {
  if (alternate) {
    showSetup();
    return;
  }
}


void BpmField::update(Encoder::Update update) {
  auto status = ClockStatus::current();

  setBpm(status.bpm + update.accel(5));
}

void BpmField::redraw() {

  display.setTextColor(foreColor());

  auto status = ClockStatus::current();

  switch (status.state) {
    case clockFreeRunning:
    case clockSyncRunning: {
      // rotated coordinates of the field
      const int16_t xr = display.height() - (y + h);
      const int16_t yr = x;
      const int16_t wr = h;
      const int16_t hr = w;

      setRotationSideways();
      centerNumber(status.bpm, xr, yr, wr, hr);
      setRotationNormal();
      break;
    }

    case clockPaused: {
      display.drawBitmap(x, y, SyncImages::paused, 15, 32, foreColor());
      break;
    }

    case clockPerplexed: {
      auto image = SyncImages::clkbad;
      switch (state.syncMode) {
        case syncExternal24ppqn:
        case syncExternal48ppqn:
          image = SyncImages::dinbad;
          break;
        default:
          break;
      }
      display.drawBitmap(x, y, image, 15, 32, foreColor());
      break;
    }
  }

  clockAsDrawn = status;
}


/**
 **  Sync Field
 **/

bool SyncField::isOutOfDate() {
  return
    (syncAsDrawn != pendingSync)
    || Field::isOutOfDate();
}

void SyncField::enter(bool alternate) {
  pendingSync = state.syncMode;
}

void SyncField::exit() {
  state.syncMode = pendingSync;
  setSync(state.syncMode);
}

namespace {
  struct SyncInfo {
    SyncMode              mode;
    const unsigned char * image;
  };

  SyncInfo syncOptions[] = {
    { syncInternal,  SyncImages::internal },
    { syncExternal1ppqn,  SyncImages::clk4 },
    { syncExternal2ppqn,  SyncImages::clk8 },
    { syncExternal4ppqn,  SyncImages::clk16 },
    { syncExternal8ppqn,  SyncImages::clk32 },
    { syncExternal24ppqn, SyncImages::din24 },
    { syncExternal48ppqn, SyncImages::din48 },
    // { syncMidi,   SyncImages::usbmidi },
  };

  const int numSyncOptions = sizeof(syncOptions) / sizeof(syncOptions[0]);

  int findSyncModeIndex(SyncMode sync) {
    for (int i = 0; i < numSyncOptions; ++i) {
      if (sync == syncOptions[i].mode)
        return i;
    }
    return -1;
  }
}

void SyncField::update(Encoder::Update update) {
  int i = findSyncModeIndex(pendingSync);
  int j = constrain(i + update.dir(), 0, numSyncOptions - 1);
  pendingSync = syncOptions[j].mode;
}

void SyncField::redraw() {
  display.setTextColor(foreColor());

  int i = findSyncModeIndex(pendingSync);
  if (i >= 0)
    display.drawBitmap(x, y, syncOptions[i].image, 15, 32, foreColor());
  else
    centerText("?", x, y, w, h);
  syncAsDrawn = pendingSync;
}


namespace ReturnImage {
  const unsigned char returnArrow[] = { // 15 x 22
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x38, 0x00, 0x38, 0x00, 0x38,
    0x00, 0x38, 0x00, 0x38, 0x02, 0x38, 0x06, 0x38, 0x0e, 0x78, 0x1f, 0xf0, 0x3f, 0xf0, 0x1f, 0xc0,
    0x0e, 0x00, 0x06, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
}

void ReturnField::enter(bool alternate) {
  showMain();
}

void ReturnField::redraw() {
  display.drawBitmap(x, y, ReturnImage::returnArrow, 15, 22, foreColor());
}

