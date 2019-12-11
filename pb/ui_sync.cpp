#include "ui_sync.h"

#include "timer_hw.h"


bool SyncField::isOutOfDate() {
  if (modeAsDrawn != mode)
    return true;

  switch (mode) {
    case displayBPM:
      if (bpmAsDrawn != currentBpm())
        return true;
      break;

    case displaySync:
      if (syncAsDrawn != pendingSync)
        return true;
      break;
  }

  return Field::isOutOfDate();
}

void SyncField::enter(bool alternate) {
  mode = alternate ? displaySync : displayBPM;
  pendingSync = state.syncMode;
}

void SyncField::exit() {
  if (mode == displaySync) {
    state.syncMode = pendingSync;
    setSync(state.syncMode);
    mode = displayBPM;
  }
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
    { sync4ppqn,  "1/16" },
    { sync8ppqn,  "1/32" },
    { sync24ppqn, "D 24" },
    { sync48ppqn, "D 48" }
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
      setBpm(currentBpm() + dir);
      break;

    case displaySync:
      int i = findSyncModeIndex(pendingSync);
      int j = constrain(i + dir, 0, numSyncOptions - 1);
      pendingSync = syncOptions[j].mode;
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
      bpmAsDrawn = currentBpm();
      centerNumber(bpmAsDrawn, xr, yr, wr, hr);
      break;

    case displaySync:
      smallText();
      int i = findSyncModeIndex(pendingSync);
      const char* s = i < 0 ? "???" : syncOptions[i].text;
      centerText(s, xr, yr, wr, hr);
      syncAsDrawn = pendingSync;
      resetText();
      break;
  }
  modeAsDrawn = mode;

  display.setRotation(0);
}
