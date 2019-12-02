#ifndef _INCLUDE_FLASH_H_
#define _INCLUDE_FLASH_H_

#include <stdint.h>



class FlashMemoryLog {
public:
    FlashMemoryLog(
      uint32_t dataLength,
      uint32_t regionStartSector,
      uint32_t regionSectorCount
      );

    bool begin();

    bool readCurrent(uint8_t* buf);
    bool writeNext(const uint8_t* buf);

private:
  const uint32_t dataLength;

  const uint32_t regionStartSector;
  const uint32_t regionEndSector;
  const uint32_t regionMagicValue;

  uint32_t currentSerial;
  uint32_t currentSector;
  uint32_t currentIndex;
};


template< typename T >
class Persistent {
public:
    Persistent(
      T& t,
      uint32_t startSector,
      uint32_t sectorCount)
      : data(reinterpret_cast<uint8_t*>(&t)),
        memoryLog(sizeof(T), startSector, sectorCount)
      { }

    inline bool begin()
      { return memoryLog.begin() && memoryLog.readCurrent(data); }

    inline bool save() { return memoryLog.writeNext(data); }

private:
    uint8_t* data;
    FlashMemoryLog memoryLog;
};




#endif // _INCLUDE_FLASH_H_
