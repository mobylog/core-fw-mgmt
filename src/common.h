#ifndef _COMMON_H
#define _COMMON_H

#include "hal.h"

#define WDG_MODE_NORMAL 1 // Reset when timeout
#define WDG_MODE_STANBY 2 // Standby when timeout

#define WDG_STANDBY_DELAY 30 // Default delay for CPU to finish tasks
#define WDG_NORMAL_DELAY 3

// Power stuff
void startCpu(void);
void stopCpu(void);
void resetCpu(void);
void standby(void);
void powerOffCallback(void *arg);

void setWdgTimeout(uint8_t timeout);
uint8_t getWdgTimeout(void);
void stopWdg(void);
void watchDogCallback(GPTDriver *gptp);

extern bool shutdown;

#endif
