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

struct State {
  Settings    settings;
  uint8_t     memoryIndex;
  uint8_t     syncMode;     // not used yet
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
