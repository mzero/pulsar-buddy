#ifndef _INCLUDE_VERSION_H_
#define _INCLUDE_VERSION_H_

//#define PB_DEVELOPMENT

#define PB_VERSION 106
/*
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
