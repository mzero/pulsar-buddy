#include "midi.h"

#include <Adafruit_TinyUSB.h>
#include <Arduino.h>

#include "clock.h"
#include "timer_hw.h"



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



  inline void midiClock() {
    softwareExtClock();
  }

  void midiPosition(q_t position) {
    setPosition(position);
    Serial.printf("setting MIDI position to %d = ", position / Q_PER_MIDI_BEAT);
    dumpQ(position);
    Serial.println();
  }

  void midiStop() {
    stopClock();
    // TODO: move position up to next MIDI beat. (?)
    Serial.println("midiStop");
    dumpClock();
  }

  void midiContinue() {
    Serial.println("midiContinue");
    clearClockHistory();
    runClock();
 }



  void checkIncomingMidi() {
    static uint32_t yieldAfter = millis() + 1;

    uint8_t packet[4];
    while (usb_midi.receive(packet)) {
      // Serial.printf("usb packet: %02x %02x %02x %02x\n",
      //   packet[0], packet[1], packet[2], packet[3]);

      switch (packet[0] & 0x0f) {  /* the CIN value */

        case 3: {
          // Three-byte System Common messages
          switch (packet[1]) {
            case 0xf2: {
              uint32_t midiBeat =
                (static_cast<uint32_t>(packet[3]) << 7)
                | static_cast<uint32_t>(packet[2]);

              midiPosition(midiBeat * Q_PER_MIDI_BEAT);
              break;
            }
          }
          break;
        }

        case 0xf: {
          // Single-byte messages, including real-time
          switch (packet[1]) {
            case 0xf8:  midiClock();      break;
            case 0xfa:  midiPosition(0);  // fall through
            case 0xfb:  midiContinue();   break;
            case 0xfc:  midiStop();       break;
            default: ;
          }
          break;
        }

      }

      if (millis() >= yieldAfter)
        break;
    }
  }

}


void initializeMidi() {
  USBDevice.setID(0x1209, 0x6f00);  // see pid.codes for this allocation
  USBDevice.setManufacturerDescriptor("electric.kitchen");
  USBDevice.setProductDescriptor("Pulsar Buddy");

  usb_midi.begin();

  USBDevice.attach();
}

void updateMidi() {
  checkMidiClock();
  checkIncomingMidi();
}


