#ifndef _EEPROM_H
#define _EEPROM_H

#include "hal.h"

#define EEPROM_DATA_SIZE 124

enum key_type {
  INVALID = 0,
  AES_CTR_128,
  AES_CTR_256,
  AES_CBC_128,
  AES_CBC_256,
  AES_ECB_128,
  AES_ECB_256,
  SHA1,
  SHA256,
  HMAC_SHA1,
  HMAC_SHA256,
};

typedef struct {
  uint8_t type;
  uint8_t padding1;
  uint8_t key[32]; // Up to 256 bits
} __attribute__((packed)) cryp_key_t;

// Up to 128 bytes
typedef struct {
  uint8_t fw_version[3];
  uint8_t hw_version[3];
  uint8_t boot_slot;
  uint8_t boot_retries;
  uint64_t last_updated; // Unix timestamp
  cryp_key_t fs_key; // 256 bit AES key for FS encryption
  uint8_t padding[74];
  uint32_t crc;
} __attribute__((packed)) eeprom_t;

_Static_assert(sizeof(eeprom_t) == 128, "eeprom_t must be 128 bytes");

uint8_t eepromCheckCRC(void);
void eepromWriteFwVersion(uint8_t version[3]);

extern eeprom_t * eeprom;

#endif
