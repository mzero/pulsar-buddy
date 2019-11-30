#include "state.h"

double bpm = 128;

namespace {
  const Settings defaultSettings = {
    .numberMeasures     = 4,
    .beatsPerMeasure    = 16,
    .beatUnit           = 16,

    .tupletCount        = 3,
    .tupletTime         = 2,
    .tupletUnit         = 4
  };

  State _userState;
  State _activeState;
}

void initializeState() {
  _userState.memoryIndex = 1;
  _userState.settings = defaultSettings;
  _activeState.memoryIndex = 1;
  _activeState.settings = defaultSettings;
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


