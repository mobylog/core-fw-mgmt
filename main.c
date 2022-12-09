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

/*
 * Heartbeat led
 */
THD_WORKING_AREA(waThread1, 64);
THD_FUNCTION(Thread1, arg) {

  (void)arg;

  while (true) {
    palSetPad(GPIOA, 10);
    chThdSleepMilliseconds(500);
    palClearPad(GPIOA, 10);
    chThdSleepMilliseconds(500);
  }
}

/*
 * Serial
 */
THD_WORKING_AREA(waThread2, 64);
THD_FUNCTION(Thread2, arg) {

  (void)arg;

  sdStart(&LPSD1, NULL);

  while (true) {
    chnWrite(&LPSD1, (const uint8_t *)"Hello World!\r\n", 14);
    chThdSleepMilliseconds(2000);
  }
}


/*===========================================================================*/
/* ADC driver related.                                                       */
/*===========================================================================*/

#define ADC_GRP1_NUM_CHANNELS       1
#define ADC_GRP1_BUF_DEPTH      8

/* Buffers are allocated with size and address aligned to the cache
   line size.*/
#if CACHE_LINE_SIZE > 0
CC_ALIGN_DATA(CACHE_LINE_SIZE)
#endif
adcsample_t samples1[CACHE_SIZE_ALIGN(adcsample_t, ADC_GRP1_NUM_CHANNELS * ADC_GRP1_BUF_DEPTH)];

/*
 * ADC streaming callback.
 */
size_t n= 0, nx = 0, ny = 0;
void adccallback(ADCDriver *adcp) {

  /* Updating counters.*/
  n++;
  if (adcIsBufferComplete(adcp)) {
    nx += 1;
  }
  else {
    ny += 1;
  }

  if ((n % 200) == 0U) {
  }
}

/*
 * ADC errors callback, should never happen.
 */
void adcerrorcallback(ADCDriver *adcp, adcerror_t err) {

  (void)adcp;
  (void)err;

  chSysHalt("it happened");
}

const ADCConfig adccfg1 = {
  .dummy        = 0U
};

/*
 * ADC conversion group 1.
 * Mode:        Linear buffer, 1 channel, SW triggered.
 * Channels:    IN10.
 */
const ADCConversionGroup adcgrpcfg1 = {
  .circular     = false,
  .num_channels = ADC_GRP1_NUM_CHANNELS,
  .end_cb       = NULL,
  .error_cb     = adcerrorcallback,
  .cfgr1        = ADC_CFGR1_CONT | ADC_CFGR1_RES_12BIT,     /* CFGR1 */
  .cfgr2        = 0,                                        /* CFGR2 */
  .tr           = ADC_TR(0, 0),                             /* TR */
  .smpr         = ADC_SMPR_SMP_19P5,                        /* SMPR */
  .chselr       = ADC_CHSELR_CHSEL1 | ADC_CHSELR_CHSEL9     /* CHSELR */
};

/*
 * ADC
 */
THD_WORKING_AREA(waThread3, 64);
THD_FUNCTION(Thread3, arg) {

  (void)arg;

  adcStart(&ADCD1, &adccfg1);
  adcSTM32EnableVREF(&ADCD1);
  adcSTM32EnableTS(&ADCD1);

  while (true) {
    adcConvert(&ADCD1, &adcgrpcfg1, samples1, ADC_GRP1_BUF_DEPTH);
    chThdSleepMilliseconds(2000);
  }
}


/*
 * Threads creation table, one entry per thread.
 */
THD_TABLE_BEGIN
  THD_TABLE_THREAD(0, "heartbeat", waThread1, Thread1, NULL)
  THD_TABLE_THREAD(1, "serial",   waThread2, Thread2, NULL)
  THD_TABLE_THREAD(2, "adc",   waThread3, Thread3, NULL)
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

  /* This is now the idle thread loop, you may perform here a low priority
     task but you must never try to sleep or wait in this loop. Note that
     this tasks runs at the lowest priority level so any instruction added
     here will be executed after all other tasks have been started.*/
  while (true) {
  }
}
