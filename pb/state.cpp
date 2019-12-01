#include "state.h"

#include <Arduino.h>


double bpm = 128;

namespace {
  State _userState;
  State _activeState;
}

State& userState() { return _userState; }
const State& activeState() { return _activeState; }

void commitState() {
  _activeState = _userState;
}

bool pendingLoop() {
  return
       _userState.settings.numberMeasures  != _activeState.settings.numberMeasures
    || _userState.settings.beatsPerMeasure != _activeState.settings.beatsPerMeasure
    || _userState.settings.beatUnit        != _activeState.settings.beatUnit
    ;
}

bool pendingTuplet() {
  return
       _userState.settings.tupletCount != _activeState.settings.tupletCount
    || _userState.settings.tupletTime  != _activeState.settings.tupletTime
    || _userState.settings.tupletUnit  != _activeState.settings.tupletUnit
    ;
}

bool pendingMemory() {
  return
      _userState.memoryIndex != _activeState.memoryIndex;
}

bool pendingState() {
  return pendingLoop() || pendingTuplet() || pendingMemory();
}



namespace {
  struct Storage_v1 {
    static const uint32_t version = 1;
    static const int numSlots = 4;

    Settings    settings[numSlots];
  };

  typedef Storage_v1 Storage;

  const Storage defaultStorage =
    { .settings =
      { { 4, 16, 16,    3, 2,  4 },
        { 4,  3,  4,    2, 3,  4 },
        { 2,  4,  4,    5, 4,  8 },
        { 2, 13, 16,    3, 2, 16 }
      }
    };

  struct StorageContainer {
    uint32_t      magic;
    uint32_t      version;
    union {
      Storage_v1  v1;
    };
  };

  const uint32_t STORAGE_MAGIC = 1284161632;

  StorageContainer container;
  inline Storage& storage() { return container.v1; }
  bool checkIndex(int i) {
    return 1 <= i && i <= Storage::numSlots;
  }

  void initializeStorage() {
    // FIXME: read storage from Flash here....

    if (container.magic == STORAGE_MAGIC) {
      if (container.version == Storage::version)
        return;

      switch (container.version) {
        // case on older versions, and if possible, convert in place and return
      }
    }

    // either nothing in flash, or couldn't convert version

    container.magic = STORAGE_MAGIC;
    container.version = Storage::version;

    storage() = defaultStorage;
  }
}

void loadFromMemory(int index) {
  if (!checkIndex(index))
    return;

  _userState.memoryIndex = index;
  _userState.settings = storage().settings[index-1];
  Serial.print("memory load from ");
  Serial.println(index);
}

void storeToMemory(int index) {
  if (!checkIndex(index))
    return;

  _userState.memoryIndex = index;
  storage().settings[index-1] = _userState.settings;
  // FIXME: write to flash here
  Serial.print("memory save into ");
  Serial.println(index);
}

void initializeState() {
  initializeStorage();
  loadFromMemory(1);
  commitState();
}

