#include "midi.h"

#include <Adafruit_TinyUSB.h>
#include <Arduino.h>


#define MIDI_STATS

namespace {
  volatile uint32_t midiClocksToSend = 0;
}

void isrMidiClock() {
  midiClocksToSend += 1;
}


namespace {
  Adafruit_USBD_MIDI usb_midi;

  void sendMidiClock() {
    const uint8_t packet[4] = { 5, 0xf8, 0, 0 };
    usb_midi.send(packet);
  }


#ifdef MIDI_STATS
  void sendMidiClockStats(uint32_t sentThisTime) {
    const int buckets = 8;
    static uint32_t sentCountHistogram[buckets] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    static uint32_t sentCountOverflow = 0;

    if (0 == sentThisTime) {
      return;
    }
    else if (0 < sentThisTime && sentThisTime <= buckets) {
      sentCountHistogram[sentThisTime - 1] += 1;
    }
    else if (sentThisTime > buckets) {
      sentCountOverflow += 1;
    }

    const uint32_t reportInterval = 6000;
    static uint32_t reportNext = reportInterval;
    uint32_t now = millis();
    if (now >= reportNext) {
      reportNext = now + reportInterval;
      Serial.println("-----------------");
      for (int i = 0; i < buckets; ++i) {
        auto n = sentCountHistogram[i];
        sentCountHistogram[i] = 0;
        if (n > 0) {
          Serial.printf("sent %d x midiClock %d times\n", i+1, n);
        }
        if (sentCountOverflow > 0) {
          Serial.printf("sent many midiClocks %d times\n", sentCountOverflow);
          sentCountOverflow = 0;
        }
      }
    }
  }
#endif

  void checkMidiClock() {
    static uint32_t midiClocksSent = 0;
    uint32_t sendThisTime = midiClocksToSend - midiClocksSent;

    for (auto i = sendThisTime; i > 0; --i)
      sendMidiClock();

    midiClocksSent += sendThisTime;

#ifdef MIDI_STATS
    sendMidiClockStats(sendThisTime);
#endif
  }

}


void initializeMidi() {
  usb_midi.begin();
  //while (!USBDevice.mounted()) delay(1);

  // force USB reconnect so MIDI port will be re-found
  // TODO: How to do this gracefully?
  USBDevice.detach();
  delay(3000);
  USBDevice.attach();

}

void updateMidi() {
  checkMidiClock();
}


