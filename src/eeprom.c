#include "eeprom.h"
#include "common.h"
#include <string.h>

eeprom_t * eeprom = (eeprom_t *)DATA_EEPROM_BASE;

/*
  Check if eeprom content is valid using CRC.
  Take into account padding data to keep the same length later on.
*/
uint8_t eepromCheckCRC() {
  uint32_t crc;

  crcAcquireUnit(&CRCD1);
  crcReset(&CRCD1);
  crc = crcCalc(&CRCD1, EEPROM_DATA_SIZE, eeprom);
  crcReleaseUnit(&CRCD1);

  return crc == eeprom->crc;
}

static void eepromUpdateCRC(void) {
  crcAcquireUnit(&CRCD1);
  crcReset(&CRCD1);
  eeprom->crc = crcCalc(&CRCD1, EEPROM_DATA_SIZE, eeprom);
  crcReleaseUnit(&CRCD1);
}

void eepromWriteFwVersion(uint8_t version[3]) {
  memcpy(eeprom->fw_version, version, 3);
  eepromUpdateCRC();
}
