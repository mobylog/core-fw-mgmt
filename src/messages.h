
#ifndef _MESSAGES_H_
#define _MESSAGES_H_

#include "ch.h"
#include "hal.h"

#define MAGIC_REQ	0xDEAD
#define MAGIC_RES	0xBEEF

#define CMD_NOP      0x00
#define CMD_INFO     0x01
#define CMD_WDG      0x02
#define CMD_OFF      0x03
#define CMD_RST      0x04
#define CMD_VOLT     0x05
#define CMD_FEL      0x06
#define CMD_GET_DATE 0x07
#define CMD_SET_DATE 0x08

typedef struct __attribute__((__packed__)) {
	uint16_t magic;
	uint8_t	 cmd;
	uint8_t	 data[32];
  uint8_t	 padding[1];
	uint32_t crc;
} mgmt_msg_t;

bool read_request(BaseChannel *chn);

#endif // _MESSAGES_H_
