#ifndef _INCLUDE_TIMER_HW_H_
#define _INCLUDE_TIMER_HW_H_

#import "state.h"


extern void initializeTimers(double, void (*onMeasure)());

extern void setTimerBpm(double);
extern void resetSync(SyncMode);
extern void syncBPM();

extern void resetTimers(const Settings&);
extern void updateTimers(const Settings&);

extern void dumpTimer();
extern void dumpCapture();

#endif // _INCLUDE_TIMER_HW_H_
