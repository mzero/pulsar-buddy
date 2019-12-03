#ifndef _INCLUDE_FLASH_H_
#define _INCLUDE_FLASH_H_

#include <stdint.h>



class FlashMemoryLog {
public:
    FlashMemoryLog(uint32_t dataLength)
      : dataLength(dataLength)
      { }

    bool begin(uint32_t startSector, uint32_t sectorCount);
    bool readCurrent(uint8_t* buf);
    bool writeNext(const uint8_t* buf);

private:
  const uint32_t dataLength;

  uint32_t regionStartSector;
  uint32_t regionEndSector;
  uint32_t regionMagicValue;

  uint32_t currentSerial;
  uint32_t currentSector;
  uint32_t currentIndex;
};


template< typename T >
class FlashLog : FlashMemoryLog {
public:
    FlashLog()
      : FlashMemoryLog(sizeof(T))
      {}

    inline bool begin(uint32_t startSector, uint32_t sectorCount)
      { return FlashMemoryLog::begin(startSector, sectorCount); }

    inline bool load(T& data)
      { return FlashMemoryLog::readCurrent(reinterpret_cast<uint8_t*>(&data)); }

    inline bool save(const T& data)
      { return FlashMemoryLog::writeNext(reinterpret_cast<const uint8_t*>(&data)); }
};




#endif // _INCLUDE_FLASH_H_
