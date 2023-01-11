#include "common.h"

bool ping_expired;
bool shutdown;
uint8_t wdgMode = WDG_MODE_NORMAL;

/* Start CPU with power sequencing */
void startCpu() {
  palSetLine(LINE_PWR_ON_IO); // VCC IO/DDR on
  palClearLine(LINE_FEL); // eMMC ON
  chThdSleepMilliseconds(10);
  palSetLine(LINE_PWR_ON_CORE); // VCore ON

  chThdSleepMilliseconds(64); // from datasheet
  palSetLine(LINE_NRST_MPU); // CPU ON

  wdgMode = WDG_MODE_STANBY;
  setWdgTimeout(WDG_NORMAL_DELAY);
}

/* Stop CPU with power sequencing */
void stopCpu() {
  stopWdg();
  palClearLine(LINE_NRST_MPU);
  palClearLine(LINE_PWR_ON_CORE);
  palClearLine(LINE_PWR_ON_IO);
}

/* Reset main CPU */
void resetCpu() {
    stopCpu();
    palSetLine(LINE_FEL); // eMMC reset
    chThdSleepMilliseconds(50);
    startCpu();
}

/* Going to standby mode */
/* This won't work if IWDG is running */
void standby() {
  stopCpu();
  chSysLock();  
  PWR->CR |= PWR_CR_CWUF | PWR_CR_CSBF;
  PWR->CR |= PWR_CR_PDDS | PWR_CR_LPDS;
  PWR->CSR |= PWR_CSR_EWUP1; // Set WKUP1 pin
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk; // Low power mode -> Deep Sleep
  SCB->SCR |= SCB_SCR_SLEEPONEXIT_Msk; // Reenter low-power mode after ISR
  __DSB();
  __WFI();
}

/* Reset timeout */
void setWdgTimeout(uint8_t timeout) {
  // if (GPTWD.state == GPT_CONTINUOUS) {
    gptStopTimer(&GPTWD);
  // }
  if (timeout > 60) {
    timeout = 60;
  }
  if (timeout < 1) {
    timeout = 1;
  }
  gptStartOneShot(&GPTWD, TIME_S2I(timeout));
}

typedef uint16_t time_secs_t; // NIL bug
uint8_t getWdgTimeout() {
  return TIME_I2S(gptGetIntervalX(&GPTWD));
}

void stopWdg() {
  if (GPTWD.state == GPT_CONTINUOUS) {
    gptStopTimer(&GPTWD);
  }
}

void powerOffCallback(void *arg) {
  (void)arg;

  chSysLockFromISR();
  // When switching to standby mode
  if (palReadLine(LINE_SYS_WKUP1) == PAL_LOW && wdgMode != WDG_MODE_STANBY)
  {
    // Debounce for 1sec then send sleep order to CPU and wait for OK
    // Force poweroff after a timer (20s default)
    wdgMode = WDG_MODE_STANBY;
    setWdgTimeout(WDG_STANDBY_DELAY);
  } else
  {
    // Cancel power-off sequence if it was a glitch
    wdgMode = WDG_MODE_NORMAL;
    setWdgTimeout(WDG_NORMAL_DELAY);
  }
  chSysUnlockFromISR();
}

void watchDogCallback(GPTDriver *gptp) {
  (void)gptp;
  if (wdgMode == WDG_MODE_NORMAL) {
    resetCpu();
  } else if (wdgMode == WDG_MODE_STANBY) {
    NVIC_SystemReset(); // Goes to standby on software reset, see main.c
  }
}
