/**
***
*** This file is meant to be included only from the middle of clock.c
***
**/


namespace {

#if DEBUG_ISR

  class DebugIsr {
    public:
      inline ~DebugIsr() {
        entry.exitState = clockState;
        entry.exitStopped = clockStopped;
        if (historyNext < historySize)
          history[historyNext++] = entry;
      }

      inline void noteQDiff(uint32_t qDiff) { entry.qDiff = qDiff; }
      inline void noteDNext(uint32_t dNext) { entry.dNext = dNext; }
      inline void noteDFilt(uint32_t dFilt) { entry.dFilt = dFilt; }
      inline void noteDAdj (uint32_t dAdj ) { entry.dAdj  = dAdj;  }

    protected:
       enum Type {
            watchdog,
            clock,
        };

      inline DebugIsr(Type t) {
        entry.type = t;
        entry.entryState = clockState;
        entry.entryStopped = clockStopped;
        entry.qDiff = 0;
        entry.dNext = 0;
        entry.dFilt = 0;
        entry.dAdj = 0;
        entry.position = currentPosition;
      }

    private:
      struct Entry {
        Type        type;
        ClockState  entryState;
        bool        entryStopped;
        ClockState  exitState;
        bool        exitStopped;
        uint32_t    qDiff;
        uint32_t    dNext;
        uint32_t    dFilt;
        uint32_t    dAdj;
        q_t         position;

        void dump(int index) const;
      };

      Entry entry;

      static const int historySize = 500;
      static Entry history[historySize];
      static int historyNext;

    public:
      static void clearHistory();
      static void dumpHistory();

    private:
      static const char* typeName(Type);
      static const char* clockStateName(ClockState, bool);
  };

  class DebugWatchdogIsr : public DebugIsr {
  public:
    inline DebugWatchdogIsr() : DebugIsr(watchdog) { }
  };

  class DebugClockIsr : public DebugIsr {
  public:
    inline DebugClockIsr() : DebugIsr(clock) { }
  };

  DebugIsr::Entry DebugIsr::history[DebugIsr::historySize];
  int DebugIsr::historyNext = 0;

  const char* DebugIsr::typeName(Type t) {
    switch (t) {
      case watchdog:  return "w-dog";
      case clock:     return "clock";
      default:        return "???";
    }
  }

  const char* DebugIsr::clockStateName(ClockState c, bool s) {
    switch (c) {
      case clockPaused:       return s ? "[||]" : "-||-";
      case clockPerplexed:    return s ? "[pp]" : "pplx";
      case clockSyncRunning:  return s ? "[sy]" : "sync";
      case clockFreeRunning:  return s ? "[fr]" : "free";
      default:                return s ? "[??]" : "????";
    }
  }

  void DebugIsr::Entry::dump(int i) const {
    Serial.printf("  [%3d] %-5s:   %-4s ",
      i, typeName(type), clockStateName(entryState, entryStopped));
    if (exitState == entryState)
      Serial.print("        ");
    else
      Serial.printf("-> %-5s", clockStateName(exitState, exitStopped));

    if (qDiff) Serial.printf("%8dqΔ", qDiff );
    else       Serial.print("      --  ");
    if (dNext) Serial.printf("%8d", dNext );
    else       Serial.print("      --");
    if (dFilt) Serial.printf("%8d", dFilt );
    else       Serial.print("      --");
    if (dAdj) {
      if      (dAdj > dFilt) Serial.printf("%8d+", dAdj-dFilt);
      else if (dAdj < dFilt) Serial.printf("%8d-", dFilt-dAdj);
      else                   Serial.printf("%8d=", 0);
    }
    else          Serial.print("      -- ");

    Serial.print("    ");
    dumpQ(position);
    Serial.println();
  }

  void DebugIsr::clearHistory() { historyNext = 0; }
  void DebugIsr::dumpHistory() {
    if (historyNext < 1) {
      Serial.println("--no ISR history--");
      return;
    }
    Serial.println("ISR history:");
    for (int i = 0; i < historyNext; ++i)
      history[i].dump(i);
    clearHistory();
  }

#else

  class DebugIsr {
  public:
      inline static void noteQDiff(uint32_t) { }
      inline static void noteDNext(uint32_t) { }
      inline static void noteDFilt(uint32_t) { }
      inline static void noteDAdj(uint32_t)  { }
      inline static void clearHistory()      { }
      inline static void dumpHistory()       { }
  };
  typedef DebugIsr DebugWatchdogIsr;
  typedef DebugIsr DebugClockIsr;

#endif

}
