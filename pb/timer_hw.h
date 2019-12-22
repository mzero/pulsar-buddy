#ifndef _INCLUDE_TIMER_HW_H_
#define _INCLUDE_TIMER_HW_H_

#include "timing.h"

void initializeTimers();

extern void isrMeasure();
extern void isrClockCapture(q_t, q_t);
extern void isrWatchdog();


typedef uint16_t divisor_t;     // the quantum timer has a 16 bit counter
float divisorToBpm(divisor_t);
divisor_t divisorFromBpm(float);
constexpr divisor_t divisorMin = 952;   // 300 bpm
constexpr divisor_t divisorMax = 9524;  // 30 bpm

void setQuantumDivisor(divisor_t);
void startQuantumEvents();
void stopQuantumEvents();

class PauseQuantum {
public:
  PauseQuantum();
  ~PauseQuantum();
private:
  bool wasRunning;
};


void readCounts(Timing&);
void writeCounts(const Timing&);
void zeroCounts();
void writePeriods(const Timing&);

void forceTriggersOff(bool);

void resetWatchdog(q_t);


void dumpTimers();

#endif // _INCLUDE_TIMER_HW_H_
