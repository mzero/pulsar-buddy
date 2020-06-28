#include "state.h"

#include <Arduino.h>

#include "flash.h"

namespace {


  template <typename T, uint32_t currentVersion, size_t maxSize>
  class Container {

  public:
    bool begin(uint32_t startSector, uint32_t sectorCount);
    void save()
      { _containerLog.save(_box); }

    T&       data()       { return _box._data; }
    const T& data() const { return _box._data; }

    static void initialize(T&);
    static bool upgrade(uint32_t, const uint8_t*, T&);

  private:
    static const uint32_t MAGIC = 1284161520 + maxSize;

    struct Box {
      uint32_t  _magic;
      uint32_t  _version;
      union {
        uint8_t _bytes[maxSize];
        T       _data;
      };

      Box() : _bytes() { }  // tell compiler which union member to construct
    };

    Box _box;

    FlashLog<Box> _containerLog;
  };


  template <typename T, uint32_t currentVersion, size_t maxSize>
  bool Container<T, currentVersion, maxSize>::
    begin(uint32_t startSector, uint32_t sectorCount) {
      _containerLog.begin(startSector, sectorCount);

      _box._magic = 0;

      if (_containerLog.load(_box)) {
        if (_box._magic == MAGIC) {
          if (_box._version == currentVersion) {
            return true;
          }

          T newData;
          if (upgrade(_box._version, _box._bytes, newData)) {
            _box._magic = MAGIC;
            _box._version = currentVersion;
            _box._data = newData;
            return true;
          }
        }
      }

      _box._magic = MAGIC;
      _box._version = currentVersion;
      initialize(_box._data);
      return false;
    }

  template <typename T, uint32_t currentVersion, size_t maxSize>
  void Container<T, currentVersion, maxSize>::
    initialize(T& data)
      { data = T(); }

  template <typename T, uint32_t currentVersion, size_t maxSize>
  bool Container<T, currentVersion, maxSize>::
    upgrade(uint32_t, const uint8_t*, T&)
      { return false; }
}




namespace {
  State _userState;             // displayed in UI
  State _activeState;           // running on timers

  State _savedUserState;        // previous state, while displaying memories
  bool previewActive = false;


  typedef Container<State, 2, 40> StateContainer;
  StateContainer stateContainer;

  struct State_v1 {
    Settings    settings;
    uint8_t     memoryIndex;
    SyncMode    syncMode;
    uint16_t    userBpm;
    uint16_t    reserved;
  };

  template<>
  bool StateContainer::upgrade(uint32_t version, const uint8_t* bytes, State& s) {
    switch (version) {
      case 1: {
        const State_v1& u = *reinterpret_cast<const State_v1*>(bytes);
        s.settings = u.settings;
        s.memoryIndex = u.memoryIndex;
        s.syncMode = u.syncMode;
        s.userBpm = u.userBpm;
        // but the defaults for pulsewidth were different:
        s.pulseWidthS = pulseDuration16;
        s.pulseWidthM = pulseDuration16;
        s.pulseWidthB = pulseDuration16;
        s.pulseWidthT = pulseDuration16;
        return true;
      }
    }
    return false;
  }


  State& _flashState = stateContainer.data();  // state pending write to flash
  unsigned long writeFlashAt = 0;

  bool sameState(const State& a, const State& b) {
    return memcmp(&a, &b, sizeof(State)) == 0;
  }
}

State& userState() { return _userState; }
const State& activeState() { return _activeState; }

bool pendingLoop() {
  if (previewActive) return false;
  return
       _userState.settings.numberMeasures  != _activeState.settings.numberMeasures
    || _userState.settings.beatsPerMeasure != _activeState.settings.beatsPerMeasure
    || _userState.settings.beatUnit        != _activeState.settings.beatUnit
    ;
}

bool pendingTuplet() {
  if (previewActive) return false;
  return
       _userState.settings.tupletCount != _activeState.settings.tupletCount
    || _userState.settings.tupletTime  != _activeState.settings.tupletTime
    || _userState.settings.tupletUnit  != _activeState.settings.tupletUnit
    ;
}

bool pendingMemory() {
  if (previewActive) return false;
  return
      _userState.memoryIndex != _activeState.memoryIndex;
}

bool pendingState() {
  if (previewActive) return false;
  return pendingLoop() || pendingTuplet() || pendingMemory();
}

void commitState() {
  _activeState = _userState;

  _flashState = _userState;
  stateContainer.save();
  writeFlashAt = 0;
}

void persistState() {
  if (previewActive)
    return;

  if (!sameState(_flashState, _userState)) {
    _flashState = _userState;
    writeFlashAt = millis() + 2000;
    return;
  }

  if (writeFlashAt && writeFlashAt <= millis()) {
    stateContainer.save();
    writeFlashAt = 0;
  }
}



namespace {
  struct Storage_v1 {
    static const int numSlots = 4;

    Settings settings[numSlots];
  };

  template<>
  void Container<Storage_v1, 1, 112>::initialize(Storage_v1& data) {
    static const Storage_v1 defaultStorage =
    { .settings =
      { { 4, 16, 16,    3, 2,  4 },
        { 4,  3,  4,    2, 3,  4 },
        { 2,  4,  4,    5, 4,  8 },
        { 2, 13, 16,    3, 2, 16 }
      }
    };

    data = defaultStorage;
  }

  Container<Storage_v1, 1, 112> storageContainer;



  typedef Storage_v1 Storage;

  inline Storage& storage() { return storageContainer.data(); }

  bool checkIndex(int i) {
    return 1 <= i && i <= Storage::numSlots;
  }

}


void loadFromMemory(int index) {
  if (!checkIndex(index))
    return;

  _userState.memoryIndex = index;
  _userState.settings = storage().settings[index-1];
}

void storeToMemory(int index) {
  if (!checkIndex(index))
    return;

  _userState.memoryIndex = index;
  storage().settings[index-1] = _userState.settings;

  storageContainer.save();
}

void showMemoryPreview(int index) {
  if (!previewActive) {
    _savedUserState = _userState;
    previewActive = true;
  }
  loadFromMemory(index);
}

void endMemoryPreview() {
  if (previewActive) {
    _userState = _savedUserState;
    previewActive = false;
  }
}

void initializeState() {
  bool stateLoaded = stateContainer.begin(0, 8);
  storageContainer.begin(8, 8);
    // The Feather M0 Express has 2MB of Flash, or 512 pages.
    // But 64KB units are cheap and common, so this is sized to fit into
    // just 16 pages. Still, at expected update rates, these will take 100
    // years to reach the minimum supported erase cycles of the chips.

  if (stateLoaded) {
    _userState = _activeState = _flashState;
  } else {
    loadFromMemory(1);
    commitState();
  }
}

