#ifndef _INCLUDE_TIMER_HW_H_
#define _INCLUDE_TIMER_HW_H_

/*
    This module works in terms of beats (B).
    In the UI, a beat is displayed as a quarter note.

    1/4 beat is the smallest "even" division, and any multiple of
    this can be then divided by [2..9] to make a tuplet.

    Hence, the smallest sub-beat unit, or quanitum (Q) is:
      1/(4*LCM(1..9)) beat = 1/10,080 B
*/

#define Q_PER_B 10080

extern void initializeTimers(double);
extern void setTimerBpm(double);
extern void dumpTimer();

#endif // _INCLUDE_TIMER_HW_H_
