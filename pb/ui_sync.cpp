#include "ui_sync.h"


bool SyncField::isOutOfDate() {
  if (modeAsDrawn != mode)
    return true;

  switch (mode) {
    case displayBPM:
      if (bpmAsDrawn != bpm)
        return true;
      break;

    case displaySync:
      if (syncAsDrawn != state.syncMode)
        return true;
      break;
  }

  return Field::isOutOfDate();
}

void SyncField::enter(bool alternate) {
  mode = alternate ? displaySync : displayBPM;
}

void SyncField::exit() {
  mode = displayBPM;
}

namespace {
  struct SyncInfo {
    SyncMode      mode;
    const char *  text;
  };

  SyncInfo syncOptions[] = {
    { syncFixed,  "Fix" },
    { syncTap,    "Tap" },
    { sync1ppqn,  "1/4" },
    { sync2ppqn,  "1/8" },
    { sync4ppqn,  "16t" },
    { sync8ppqn,  "32t" },
    { sync24ppqn, "24d" },
    { sync48ppqn, "48d" }
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

void SyncField::update(int dir) {
  switch (mode) {
    case displayBPM:
      bpm += dir;
      break;

    case displaySync:
      int i = findSyncModeIndex(state.syncMode);
      int j = constrain(i + dir, 0, numSyncOptions - 1);
      state.syncMode = syncOptions[j].mode;
      break;
  }
}

void SyncField::redraw() {
  // rotated coordinates of the field
  int16_t xr = display.height() - (y + h);
  int16_t yr = x;
  int16_t wr = h;
  int16_t hr = w;

  display.setRotation(3);
  display.setTextColor(foreColor());

  switch (mode) {
    case displayBPM:
      centerNumber(bpm, xr, yr, wr, hr);
      bpmAsDrawn = bpm;
      break;

    case displaySync:
      int i = findSyncModeIndex(state.syncMode);
      const char* s = i < 0 ? "???" : syncOptions[i].text;
      centerText(s, xr, yr, wr, hr);
      syncAsDrawn = state.syncMode;
      break;
  }
  modeAsDrawn = mode;

  display.setRotation(0);
}
