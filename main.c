/*
    ChibiOS - Copyright (C) 2006..2018 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "hal.h"
#include "ch.h"
#include "messages.h"

bool ping_expired;
bool shutdown;

/*
 * Watchdog deadline set to more than 2.3s (LSI=37000 / (16 * 1000)).
 */
static const WDGConfig wdgcfg = {
  .pr           = STM32_IWDG_PR_16,
  .rlr          = STM32_IWDG_RL(1000),
};

/*
 * Heartbeat led
 */
THD_WORKING_AREA(waThreadHB, 64);
THD_FUNCTION(ThreadHB, arg) {

  (void)arg;

  wdgStart(&WDGD1, &wdgcfg);

  while (true) {
    palSetLine(LINE_MGMT_LED);
    chThdSleepMilliseconds(500);
    palClearLine(LINE_MGMT_LED);
    chThdSleepMilliseconds(500);
    wdgReset(&WDGD1);
  }
}

/*
 * Serial
 */
THD_WORKING_AREA(waThreadSer, 128);
THD_FUNCTION(ThreadSer, arg) {

  (void)arg;

  sdStart(&LPSD1, NULL);

  while (true) {
    read_message((BaseChannel*)&LPSD1);
  }
}

/*
 * CPU watchdog
 */
THD_WORKING_AREA(waThreadWD, 64);
THD_FUNCTION(ThreadWD, arg) {

  (void)arg;

  sdStart(&LPSD1, NULL);

  // Power ON
  chThdSleepMilliseconds(10);
  palSetLine(LINE_PWR_ON_IO);
  chThdSleepMilliseconds(10);
  palSetLine(LINE_PWR_ON_CORE);
  chThdSleepMilliseconds(10);
  palSetLine(LINE_NRST_MPU);

  gptStartOneShot(&GPTD2, TIME_MS2I(1000));

  while (true) {
    while (!ping_expired) {

      if (shutdown) {
        palClearLine(LINE_NRST_MPU);
        palClearLine(LINE_PWR_ON_IO);
        palClearLine(LINE_PWR_ON_CORE);

        /* going to standby mode */
        chSysLock();
        PWR->CSR |= PWR_CSR_EWUP1; // Set WKUP1 pin
        PWR->CR |= PWR_CR_CWUF | PWR_CR_CSBF;
        PWR->CR |= PWR_CR_PDDS | PWR_CR_LPDS;
        SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
        __WFI();
      }

      chThdSleepMilliseconds(100);
      continue;
    }

    palClearLine(LINE_NRST_MPU);
    palSetLine(LINE_FEL); // eMMC NRST
    chThdSleepMilliseconds(10);

    palClearLine(LINE_FEL);
    chThdSleepMilliseconds(100);
    palSetLine(LINE_NRST_MPU);
    chThdSleepMilliseconds(10);
  }
}

/*===========================================================================*/
/* ADC driver related.                                                       */
/*===========================================================================*/

#define ADC_GRP1_NUM_CHANNELS 2
#define ADC_GRP1_BUF_DEPTH    8

/* Buffers are allocated with size and address aligned to the cache
   line size.*/
adcsample_t samples1[CACHE_SIZE_ALIGN(adcsample_t, ADC_GRP1_NUM_CHANNELS * ADC_GRP1_BUF_DEPTH)];

/*
 * ADC errors callback, should never happen.
 */
static void adcerrorcallback(ADCDriver *adcp, adcerror_t err) {

  (void)adcp;
  (void)err;
}

static const ADCConfig adccfg1 = {
  .dummy = 0U
};

static const ADCConversionGroup adcgrpcfg1 = {
  .circular     = true,
  .num_channels = ADC_GRP1_NUM_CHANNELS,
  .end_cb       = NULL,
  .error_cb     = adcerrorcallback,
  .cfgr1        = ADC_CFGR1_CONT |
                  ADC_CFGR1_RES_12BIT |
                  ADC_CFGR1_EXTEN_RISING |
                  ADC_CFGR1_EXTSEL_SRC(0),                  /* CFGR1 */
  .cfgr2        = 0,                                        /* CFGR2 */
  .tr           = ADC_TR(0, 0),                             /* TR */
  .smpr         = ADC_SMPR_SMP_160P5,                       /* SMPR */
  .chselr       = ADC_CHSELR_CHSEL1 | ADC_CHSELR_CHSEL9     /* CHSELR */
};

static void gpt_cb(GPTDriver *gptp) {
  (void)gptp;
  ping_expired = true;
}

const GPTConfig gptcfg1 = {
  .frequency    =  10000U,
  .callback     =  gpt_cb,
  .cr2          =  0U,
  .dier         =  0U
};

static void pwr_cb(void *arg) {
  (void)arg;

  chSysLockFromISR();
  if (palReadLine(LINE_SYS_WKUP1) == PAL_LOW)
  {
    // Debounce for 1sec then send sleep order to CPU and wait for OK
    // Force poweroff after a timer (20s default)
  } else
  {
    // Ignore
  }
  chSysUnlockFromISR();
}


/*
 * Threads creation table, one entry per thread.
 */
THD_TABLE_BEGIN
  THD_TABLE_THREAD(0, "heartbeat", waThreadHB, ThreadHB, NULL)
  THD_TABLE_THREAD(1, "cpu-watchdog", waThreadWD, ThreadWD, NULL)
  THD_TABLE_THREAD(2, "serial", waThreadSer, ThreadSer, NULL)
THD_TABLE_END

/*
 * Application entry point.
 */
int main(void) {

  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  ping_expired = false;
  shutdown = false;

  palEnableLineEvent(LINE_SYS_WKUP1, PAL_EVENT_MODE_BOTH_EDGES);
  palSetLineCallback(LINE_SYS_WKUP1, pwr_cb, NULL);

  palClearLine(LINE_NRST_MPU);
  palClearLine(LINE_PWR_ON_IO);
  palClearLine(LINE_PWR_ON_CORE);

  adcStart(&ADCD1, &adccfg1);
  adcSTM32EnableVREF(&ADCD1);
  adcSTM32EnableTS(&ADCD1);

  adcStartConversion(&ADCD1, &adcgrpcfg1, samples1, ADC_GRP1_BUF_DEPTH);

  gptStart(&GPTD2, &gptcfg1);

  /* Start threads manually at the end */
  chThdCreate(&nil_thd_configs[0]);
  chThdCreate(&nil_thd_configs[1]);

  /* This is now the idle thread loop, you may perform here a low priority
     task but you must never try to sleep or wait in this loop. Note that
     this tasks runs at the lowest priority level so any instruction added
     here will be executed after all other tasks have been started.*/
  while (true) {
  }
}
