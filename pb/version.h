#ifndef _INCLUDE_VERSION_H_
#define _INCLUDE_VERSION_H_

#define PB_DEVELOPMENT
//#define PB_BETA

#define PB_VERSION 108

// PB_VERSION 107
/* 2020-10-29
  - user facing:
    - added option to sync to MIDI (over USB)
      - handle both "clock while stopped" and "no clock when stopped"
      - handle song position (SPP)
      - handle DAW startup sequences from non-sixteenth note positions
    - disabled extended bpm option, it's always extended now
    - improved display when paused or stopped
    - flip display option
  - internal:
    - complete rework of the clock - much cleaner now
    - introduction of concept of position, and stopped for clock
*/


// PB_VERSION 106
/* 2020-07-13
  - user facing:
    - settings page UI
    - pulse width control
  - internal:
    - trigger measure changes off the Sequence timer to leave Measure time free
    - settings data structure updated to support pulse widths & future output
      pin assignments
    - reduce speed of flash SPI clock to improve reliability on rev. E boards
    - increase size of flash log area for Settings & State, migrating older
      settings if present
    - flash erase and test feature in configuration screen
*/

// PB_VERSION 105
/* 2020-06-06
  - support flash chip used in SMT board
  - tiny clean up of code in critical.cpp
*/

// PB_VERSION 104
/* 2020-04-09
  - added hardware test mode
  - updated boards and libraries
*/

// PB_VERSION 103
/* 2020-01-17
  - extended BPM range option
  - improve long press behavior
*/

// PB_VERSION 102
/* 2020-01-17
  - first version with a version
*/


#endif // _INCLUDE_VERSION_H_
