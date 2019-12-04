#ifndef _INCLUDE_STATE_H_
#define _INCLUDE_STATE_H_

#include <stdint.h>

/* SETTINGS */

struct Settings {
  uint8_t numberMeasures;
  uint8_t beatsPerMeasure;
  uint8_t beatUnit;

  uint8_t tupletCount;
  uint8_t tupletTime;
  uint8_t tupletUnit;
};


/* STATE */

enum SyncMode : uint8_t {
  syncFixed = 0,
  syncTap = 1,

  // sync based on incoming clock
  syncExternalFlag = 0x80,  // high bit indicates external sync
  syncPPQNMask = 0x7f,      // lower 7 bits are parts per beat
    // -- defined for generality, but only the values below are supported

  sync1ppqn = 0x81,    // clock = quarter note (beat)
  sync2ppqn = 0x82,    // clock = eigth note
  sync4ppqn = 0x84,    // clock = sixteenth note
  sync8ppqn = 0x88,    // clock = 32nd note (Pulsar sync)
  sync24ppqn = 0x98,   // clock = 24ppqn DIN sync
  sync48ppqn = 0xb0,   // clock = 48ppqn DIN sync
};


struct State {
  Settings    settings;
  uint8_t     memoryIndex;
  SyncMode    syncMode;     // not used yet
  uint16_t    userBpm;      // not used yet
  uint16_t    reserved;
};

// Actual state is buffered: The user may have made changes, but they aren't
// active until the commited.

State& userState();
  // the state the user wants, can be freely modified by UI code

const State& activeState();
  // the state that is currently active, used by the timer engine

inline       Settings& userSettings()   { return userState().settings; }
inline const Settings& activeSettings() { return activeState().settings; }


void commitState();
  // make the user state the active state

// These are true if the user state differs from the active state.
bool pendingLoop();
bool pendingTuplet();
bool pendingMemory();
bool pendingState();

// BPM is conceptually part of the state, but it isn't buffered like State.
extern double bpm;


void loadFromMemory(int index);
void storeToMemory(int index);
  // memory indexes are numbered from 1, because that's how users roll.

void showMemoryPreview(int index);
void endMemoryPreview();

void initializeState();

#endif // _INCLUDE_STATE_H_
