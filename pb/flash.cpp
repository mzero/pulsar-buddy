#include "flash.h"

#include "Adafruit_SPIFlash.h"


#define DEBUG 1
#if DEBUG
  #define LOG(x)    Serial.print(x)
  #define LOGLN(x)  Serial.println(x)
#else
  #define LOG(x)
  #define LOGLN(x)
#endif

namespace {

  #if defined(__SAMD51__) || defined(NRF52840_XXAA)
    Adafruit_FlashTransport_QSPI flashTransport(PIN_QSPI_SCK, PIN_QSPI_CS, PIN_QSPI_IO0, PIN_QSPI_IO1, PIN_QSPI_IO2, PIN_QSPI_IO3);
  #else
    #if (SPI_INTERFACES_COUNT == 1 || defined(ADAFRUIT_CIRCUITPLAYGROUND_M0))
      Adafruit_FlashTransport_SPI flashTransport(SS, &SPI);
    #else
      Adafruit_FlashTransport_SPI flashTransport(SS1, &SPI1);
    #endif
  #endif

  // Wrapper for Adafruit_SPIFlash::writeBuffer() that
  // correctly handles non-page aligned addresses
  uint32_t writeBuffer (
    Adafruit_SPIFlash& flash,
    uint32_t address, uint8_t const *buffer, uint32_t len)
  {
    uint32_t remain = len;

    // write one page at a time
    while(remain)
    {
      uint32_t const leftOnPage
        = SFLASH_PAGE_SIZE - (address & (SFLASH_PAGE_SIZE - 1));

      uint32_t const toWrite = min(remain, leftOnPage);

      if (flash.writeBuffer(address, buffer, toWrite) != toWrite) break;

      remain -= toWrite;
      buffer += toWrite;
      address += toWrite;
    }

    return len - remain;
  }

  Adafruit_SPIFlash flash(&flashTransport);
  bool flashBegun = false;


  static const uint32_t magicSeed = 1176328692;

  uint32_t computeMagicValue(uint32_t a, uint32_t b, uint32_t c) {
    return
      magicSeed
      ^ (a >> 4) ^ a ^ (a << 5) ^ (a << 11) ^ (a << 18) ^ (a << 26)
      ^ (b >> 8) ^ b ^ (b << 9) ^ (b << 19) ^ (b << 30)
      ^ (c >> 6) ^ c ^ (c << 7) ^ (c << 15) ^ (c << 24)
      ;
  }

  struct Header {
    uint32_t magic;
    uint32_t serial;
  };
}


FlashMemoryLog::FlashMemoryLog(
    uint32_t dataLength,
    uint32_t regionStartSector,
    uint32_t regionSectorCount
  )
  : entryLength(dataLength + sizeof(Header)),
    regionStartSector(regionStartSector),
    regionEndSector(regionStartSector + regionSectorCount),
    regionMagicValue(
      computeMagicValue(entryLength, regionStartSector, regionSectorCount)),
    currentSerial(0),   // 0 means no value found in log at all
    currentSector(0),
    currentAddress(0)
  { }

bool FlashMemoryLog::begin()
{
  if (!flashBegun) {
    if (!flash.begin()) {
      LOGLN("flash begin failure");
      return false;
    }
    LOG("flash length in sectors: "); LOGLN(flash.size() / SFLASH_SECTOR_SIZE);
    flashBegun = true;
  }


  if (entryLength > SFLASH_SECTOR_SIZE) {
    LOGLN("data length too big");
    return false;
  }

  if ((regionEndSector - regionStartSector) < 2) {
    LOGLN("must have at least 2 sectors in region");
    return false;
  }

  uint32_t numberOfSectors = flash.size() / SFLASH_SECTOR_SIZE;

  if (regionStartSector >= numberOfSectors) {
    LOGLN("start sector beyond flash capacity");
    return false;
  }

  if (regionEndSector > numberOfSectors) {
    LOGLN("region extends past end of flash");
    return false;
  }


  for (uint32_t sector = regionStartSector;
       sector < regionEndSector;
       ++sector)
  {
    Header header;
    if (flash.readBuffer(sector * SFLASH_SECTOR_SIZE,
          reinterpret_cast<uint8_t*>(&header),
          sizeof(header))
        != sizeof(header))
    {
      LOG("flash read failure at sector ");
      LOGLN(sector);
      return false;
    }

    if (header.magic != regionMagicValue)
      // can't be our data from here on after
      break;

    if (header.serial > currentSerial) {
      currentSerial = header.serial;
      currentSector = sector;
      currentAddress = currentSector * SFLASH_SECTOR_SIZE;
    }
  }

  if (currentSerial > 0) {
    uint32_t lastSectorAddress =
      currentAddress + SFLASH_SECTOR_SIZE - entryLength + 1;

    for (uint32_t probeAddress = currentAddress + entryLength;
        probeAddress < lastSectorAddress;
        probeAddress += entryLength)
    {
      Header header;
      if (flash.readBuffer(probeAddress,
            reinterpret_cast<uint8_t*>(&header),
            sizeof(header))
          != sizeof(header))
      {
        LOG("flash read failure at address ");
        LOGLN(probeAddress);
        return false;
      }

      if (header.magic != regionMagicValue)
        break;

      if (header.serial > currentSerial) {
        currentSerial = header.serial;
        currentAddress = probeAddress;
      }
    }
  }

  LOGLN("FlashMemoryLog begin:");
  LOG("   entry length =   "); LOGLN(entryLength);
  LOG("   current serial = "); LOGLN(currentSerial);
  LOG("   current sector = "); LOGLN(currentSector);
  LOG("   current offset = "); LOGLN(currentAddress & (SFLASH_SECTOR_SIZE - 1));
  return true;
}


bool FlashMemoryLog::readCurrent(uint8_t* buf)
{
  uint32_t dataLength = entryLength - sizeof(Header);

  return
    flash.readBuffer(currentAddress + sizeof(Header), buf, dataLength)
    == dataLength;
}

bool FlashMemoryLog::writeNext(const uint8_t* buf)
{
  uint32_t nextSerial = currentSerial + 1;

  if (nextSerial == 0) {
    LOG("serial number wrapped... inconcievable!");
    return false;
  }

  bool needsErase = false;
  uint32_t nextSector;
  uint32_t nextAddress;

  if (nextSerial == 1) {
    needsErase = true;
    nextSector = regionStartSector;
    nextAddress = nextSector * SFLASH_SECTOR_SIZE;
  } else {
    nextSector = currentSector;
    nextAddress = currentAddress + entryLength;

    if ((nextAddress + entryLength - 1) / SFLASH_SECTOR_SIZE != currentSector) {
      needsErase = true;
      nextSector = currentSector + 1;
      if (nextSector >= regionEndSector) {
        nextSector = regionStartSector;   // wrap
      }
      nextAddress = nextSector * SFLASH_SECTOR_SIZE;
    }
  }
  if (needsErase) {
    if (!flash.eraseSector(nextSector)) {
      LOG("sector failed to erase: ");
      LOGLN(nextSector);
      return false;
    }
  }

  Header header;
  header.magic = regionMagicValue;
  header.serial = nextSerial;

  if (writeBuffer(flash, nextAddress, reinterpret_cast<uint8_t*>(&header), sizeof(Header))
    != sizeof(Header))
  {
    LOG("header write of failed to address: ");
    LOGLN(nextAddress);
    return false;
  }

  uint32_t dataLength = entryLength - sizeof(Header);

  if (writeBuffer(flash, nextAddress + sizeof(Header), buf, dataLength)
      != dataLength)
  {
    LOG("data write of failed to address: ");
    LOGLN(nextAddress);
    return false;
  }

  currentSerial = nextSerial;
  currentSector = nextSector;
  currentAddress = nextAddress;

  LOGLN("FlashMemoryLog wrote:");
  LOG("   entry length =   "); LOGLN(entryLength);
  LOG("   current serial = "); LOGLN(currentSerial);
  LOG("   current sector = "); LOGLN(currentSector);
  LOG("   current offset = "); LOGLN(currentAddress & (SFLASH_SECTOR_SIZE - 1));
  return true;
}


