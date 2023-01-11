#include "messages.h"
#include "common.h"
#include <string.h>

static mgmt_msg_t request, response;
static RTCDateTime rtc_time;
extern uint8_t wdgMode;

static const mgmt_msg_t default_response = {
  .magic = 0xBEEF,
  .cmd = CMD_NOP,
  .data = {0},
  .crc = 0
};

bool read_request(BaseChannel *chn) {

  // Reset response var
  memcpy(&response, &default_response, sizeof(response));

  // Request from CPU
  chnRead(chn, (uint8_t*)&request, sizeof(mgmt_msg_t));

  // Check request magic header
  if (request.magic != (uint16_t)0xDEAD) {
    goto flush;
  }

  // Check CRC
  crcReset(&CRCD1);
  if (crcCalc(&CRCD1, sizeof(request) - sizeof(request.crc), (uint8_t*)&request) != request.crc) {
    goto flush;
  }

  // Process request
  response.cmd = request.cmd;
  switch (request.cmd)
  {
  case CMD_NOP:
  case CMD_INFO:
    break;
  case CMD_WDG:
    // Arg 1 is timeout in seconds 1-60
    setWdgTimeout(request.data[0]);
    response.data[0] = getWdgTimeout();
    response.data[1] = wdgMode; // Send mode to prepare for shutdown
    break;
  case CMD_FEL:
    resetCpu();
    palSetLine(LINE_FEL); // eMMC/SPI OFF to force FEL mode
    stopWdg(); // No WDG messages in FEL mode
    chThdSleepMilliseconds(1000);
    
    // Reset CPU
    // Fel pin to 1
    // Release CPU reset
    // Wait 1s
    // Fel pin to 0
    break;
  case CMD_GET_DATE:
    rtcGetTime(&RTCD1, &rtc_time);
    memcpy(response.data, (uint8_t*)&rtc_time, sizeof(rtc_time));
    break;
  case CMD_SET_DATE:
    break;
  default:
    break;
  }

  // Compute response CRC and reply
  response.crc = crcCalc(&CRCD1, sizeof(response) - sizeof(response.crc), (uint8_t*)&response);
  return chnWrite(chn, (uint8_t*)&response, sizeof(response)) == sizeof(response);

  flush:
  // Empty RX buffer
  chnReadTimeout(chn, (uint8_t*)&request, sizeof(mgmt_msg_t), TIME_IMMEDIATE);
  return false;
}
