#include "flash_reset.h"

#include "Adafruit_SPIFlashBase.h"

#include "controls.h"
#include "display.h"
#include "flash.h"

namespace {

  const size_t BLOCKSIZE = SFLASH_SECTOR_SIZE;  // should be same or less
  const size_t WORDCOUNT = BLOCKSIZE/sizeof(uint32_t);

  union TestData {
    uint32_t words[WORDCOUNT];
    uint8_t bytes[BLOCKSIZE];
  };

  uint32_t sequenceStep(uint32_t x) {
    uint64_t x_ = static_cast<uint64_t>(x);

    x_ = (x_ * 1815976680ULL) % 4294967291ULL;
      // from "Tables of Linear Congruential Generators of Different Sizes and
      // Good Lattice Structure" - Peter L'Ecuyer, Mathematics of Computation,
      // Volume 68, Number 225, January 1999

    return static_cast<uint32_t>(x_);
  }

  inline uint32_t seed(uint32_t blkno) { return blkno + 100; }

  void fillTestBlock(uint32_t blkno, TestData& t) {
    uint32_t n = seed(blkno);
    for (size_t i = 0; i < WORDCOUNT; ++i) {
      t.words[i] = n;
      n = sequenceStep(n);
    }
  }

  bool checkBlock(uint32_t blkno, TestData& t) {
    int numBad = 0;

    uint32_t n = seed(blkno);
    for (size_t i = 0; i < WORDCOUNT; ++i) {
      if (t.words[i] != n) {
        numBad += 1;
        if (numBad == 1) {
          Serial.printf("verification failed at block %4d [%4d]: ", blkno, i);
          Serial.printf("expected %08x ^ actual %08x == %08x\n",
            n, t.words[i], n ^ t.words[i]);
        }
      }
      n = sequenceStep(n);
    }

    if (numBad > 1)
      Serial.printf("  and %d other mismatches\n", numBad - 1);
    return numBad == 0;
  }


  uint32_t displayLastShown = 0;

  bool updateDisplay() { return (millis() - displayLastShown) > 133; }
  void showDisplay() {display.display(); displayLastShown = millis(); yield(); }

  const uint16_t XTAB = 65;

  class FlashReset {
  public:
    FlashReset() : state(confirmMin) { }

  private:

#if 0
    bool erase() {
      display.setCursor(0, 0);
      display.print("erasing...");
      display.setCursor(XTAB, 0);
      showDisplay();

      if (!flash.eraseChip()) {
        display.print("failed\n");
        showDisplay();
        return false;
      }

      flash.waitUntilReady();
      display.print("done\n");
      showDisplay();
      return true;
    }
#endif

    const uint32_t MAX_BLOCK_COUNT = 256;  // set to 0 for all

    void clearCounts(uint16_t y) {
      display.fillRect(XTAB, y, 128 - XTAB, 8, BLACK);
    }
    void displayCounts(
        uint16_t y, int count, int badCount = 0, bool forceBad = false) {
      clearCounts(y);
      display.setCursor(XTAB, y);
      if (badCount > 0 || forceBad) {
        display.print(badCount);
        display.print('/');
      }
      display.print(count);
      showDisplay();
    }

    void writeTestData() {
      const uint16_t YPOS = 0;

      display.setCursor(0, YPOS);
      display.print("writing");
      showDisplay();

      TestData t;
      int count = 0;

      uint32_t flashsize = flash.size();
      uint32_t addr = 0;
      while (addr < flashsize) {
        fillTestBlock(count, t);
        flash.eraseSector(addr / SFLASH_SECTOR_SIZE);
        flash.writeBuffer(addr, t.bytes, BLOCKSIZE);
        count += 1;
        addr += SFLASH_SECTOR_SIZE;

        if (updateDisplay())
          displayCounts(YPOS, count);

        if (MAX_BLOCK_COUNT && count >= MAX_BLOCK_COUNT) break;
      }

      displayCounts(YPOS, count);
    }

    void verifyTestData() {
      const uint16_t YPOS = 8;

      display.setCursor(0, YPOS);
      display.print("verifying");
      clearCounts(YPOS+8);
      showDisplay();

      TestData t;
      int count = 0;
      int badCount = 0;

      uint32_t flashsize = flash.size();
      uint32_t addr = 0;
      while (addr < flashsize) {
        flash.readBuffer(addr, t.bytes, BLOCKSIZE);
        if (!checkBlock(count, t)) {
          badCount += 1;
        }
        count += 1;
        addr += SFLASH_SECTOR_SIZE;

        if (updateDisplay())
          displayCounts(YPOS, count, badCount);

        if (MAX_BLOCK_COUNT && count >= MAX_BLOCK_COUNT) break;

        yield();  // in case Serial needs to do some work
      }

      displayCounts(YPOS, count, badCount, true);
      display.setCursor(XTAB, YPOS+8);
      display.print(badCount == 0 ? "PASS" : "FAIL");
      showDisplay();
    }

    enum State {
      confirmMin = 0,
      confirmMax = 10,

      complete = 99
    };

    bool confirming() { return confirmMin <= state && state <= confirmMax; }

    State state;

    void drawConfirm() {
      display.clearDisplay();
      display.setTextColor(WHITE, BLACK);

      display.setCursor(10, 8);
      display.print("cancel");

      display.setCursor(82, 8);
      display.print("erase");

      display.drawFastHLine(10, 20, 100, WHITE);

      int16_t x = (state - confirmMin) * 100 / (confirmMax - confirmMin) + 10;
      display.fillCircle(x, 20, 3, WHITE);
      if (state != confirmMin && state != confirmMax)
        display.fillCircle(x, 20, 2, BLACK);

      display.display();
    }


    bool loop() {
      auto u = encoder.update();
      if (u.active()) {
        if (confirming()) {
          state = static_cast<State>(constrain(state + u.dir(), confirmMin, confirmMax));
          drawConfirm();
        }
      }
      auto s = encoderButton.update();
      if (s == Button::Down) {
        switch (state) {

        case confirmMin:
          return true;

        case confirmMax:
          state = complete;

          display.clearDisplay();
          display.setTextWrap(false);

          writeTestData();
          verifyTestData();
          break;

        case complete:
          verifyTestData();
          break;

        default:
          ;
        }
      }
      return false;
    }

  public:
    void run() {
      drawConfirm();
      while (!loop()) yield();
    }

  };
}

void flashTestAndReset() {
  FlashReset f;
  f.run();
}


