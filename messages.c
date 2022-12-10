
#include "messages.h"

static uint8_t ping_val;
extern bool ping_expired;
uint16_t gcrc;

static void update_wdg(const mgmt_msg_t * msg) {
  const uint8_t expected = ~ping_val;
  if (msg->data[0] == expected) {
    ping_expired = false;
  }
}


/*
 * CRC16 configuration with DMA
 */
static const CRCConfig crc16_config = {
  .poly_size         = 16,
  .poly              = 0xA001,
  .initial_val       = 0x0,
  .final_val         = 0x0,
  .reflect_data      = 1,
  .reflect_remainder = 1,
  .end_cb = 0
};

uint8_t rxbuf[32];
bool read_message(BaseChannel *chn) {

  const mgmt_msg_t *rx_msg = (mgmt_msg_t*)rxbuf;
  mgmt_msg_t tx_msg = {
    .magic = 0xDEAD,
    .data = {0},
    .crc = 0,
    .cmd = CMD_NOP
  };

  chnRead(chn, rxbuf, 4);
  if (rx_msg->magic != (uint16_t)0xBEEF) {
    chnReadTimeout(chn, rxbuf, sizeof(rxbuf), TIME_IMMEDIATE); // Empty buffer
    return false;
  }
  chnRead(chn, rxbuf + 4, sizeof(mgmt_msg_t) - 4);

  crcReset(&CRCD1);
  if (crcCalc(&CRCD1, sizeof(rx_msg->data) + 1, &rx_msg->cmd) != rx_msg->crc) {
    return false;
  }

  switch (rx_msg->cmd)
  {
  case CMD_NOP:
    break;
  case CMD_WDG:
    update_wdg(rx_msg);
    break;
  default:
    break;
  }

  return true;
}

void setup_link(void) {
    crcAcquireUnit(&CRCD1);             /* Acquire ownership of the bus.    */
    crcStart(&CRCD1, &crc16_config);           /* Activate CRC driver              */
    crcReset(&CRCD1);
}