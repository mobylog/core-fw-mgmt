
#ifndef _MESSAGES_H_
#define _MESSAGES_H_

#include "ch.h"
#include "hal.h"

#define MAGIC_TX 0xDEAD
#define MAGIC_RX 0xBEEF

#define CMD_NOP  0x00
#define CMD_PING 0x01
#define CMD_WDG  0x02
#define CMD_OFF  0x03
#define CMD_RST  0x04
#define CMD_VOLT 0x05
#define CMD_FEL  0x06

typedef struct {
  uint16_t magic;
  uint8_t  cmd;
  uint8_t  data[3];
  uint16_t crc;
} mgmt_msg_t;

bool read_message(BaseChannel *chn);

#endif // _MESSAGES_H_
