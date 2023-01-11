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
#include "common.h"

/*
 * Watchdog window set to between 0.69-1.38s
 * (1/37000) x PR x (RL+1) -> (1/37000) x 128 x 401
 */
static const WDGConfig wdgcfg = {
  .pr   = STM32_IWDG_PR_128,
  .rlr  = STM32_IWDG_RL(400),
  .winr = STM32_IWDG_WIN(200)
};

/*
 * CRC32 configuration
 */
static const CRCConfig crc32_config = {
  .poly_size         = 32,
  .poly              = 0x04C11DB7,
  .initial_val       = 0xFFFFFFFF,
  .final_val         = 0xFFFFFFFF,
  .reflect_data      = 1,
  .reflect_remainder = 1
};

#define LEDVAL(x) pwmEnableChannel(&PWMD_MGMT_LED, 0, PWM_PERCENTAGE_TO_WIDTH(&PWMD_MGMT_LED, x))

/*
 * Heartbeat led
 */
THD_WORKING_AREA(waThreadHB, 32);
THD_FUNCTION(ThreadHB, arg) {

  (void)arg;
  LEDVAL(2000);

  // Go to standby directly if power on pin is down
  if (palReadLine(LINE_SYS_WKUP1) == PAL_LOW) {
    chThdSleepMilliseconds(2000);
    standby();
  }

  startCpu();
  wdgStart(&WDGD1, &wdgcfg);

  while (true) {
    LEDVAL(1000);
    chThdSleepMilliseconds(100);
    LEDVAL(200);
    chThdSleepMilliseconds(900);
    wdgReset(&WDGD1);
  }
}

/*
 * Serial
 */
THD_WORKING_AREA(waThreadSer, 200);
THD_FUNCTION(ThreadSer, arg) {

  (void)arg;

  while (true) {
    read_request((BaseChannel*)&LPSD1);
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

static const GPTConfig gptcfg1 = {
  .frequency    =  1000U,
  .callback     =  watchDogCallback,
  .cr2          =  0U,
  .dier         =  0U
};

static const PWMConfig pwmcfg = {
  100000, /* 100kHz PWM clock frequency. */
  1000,   /* Initial PWM period 0.1S.    */
  NULL,
  {
   {PWM_OUTPUT_ACTIVE_HIGH, NULL},
   {PWM_OUTPUT_DISABLED, NULL},
   {PWM_OUTPUT_DISABLED, NULL},
   {PWM_OUTPUT_DISABLED, NULL}
  },
  0,
  0,
  0
};

/*
 * Threads creation table, one entry per thread.
 */
THD_TABLE_BEGIN
  THD_TABLE_THREAD(0, "heartbeat", waThreadHB, ThreadHB, NULL)
  THD_TABLE_THREAD(1, "serial", waThreadSer, ThreadSer, NULL)
THD_TABLE_END

/*
 * Application entry point.
 */
int main(void) {

  uint32_t reset = RCC->CSR;
  RCC->CSR |= RCC_CSR_RMVF; // Clear reset register to avoid poluting next start.

  halInit();
  chSysInit();

  /* Enable Wake-up (VCC_ON) pin */
  palEnableLineEvent(LINE_SYS_WKUP1, PAL_EVENT_MODE_BOTH_EDGES);
  palSetLineCallback(LINE_SYS_WKUP1, powerOffCallback, NULL);

  /*!< Watchdog reset flag */
  if (reset & RCC_CSR_IWDGRSTF || reset & RCC_CSR_WWDGRSTF) {
    // Do something?
  }

  /* Wakeup from standby */
  if (reset & RCC_CSR_LPWRRSTF) {

  }

  /*!< Software Reset flag */
  if (reset & RCC_CSR_SFTRSTF) {
    /* Reset to standby (needed to disable IWDG) */
    standby();
  }

  stopCpu();

  adcStart(&ADCD1, &adccfg1);
  adcSTM32EnableVREF(&ADCD1);
  adcSTM32EnableTS(&ADCD1);

  adcStartConversion(&ADCD1, &adcgrpcfg1, samples1, ADC_GRP1_BUF_DEPTH);

  gptStart(&GPTD2, &gptcfg1);
  pwmStart(&PWMD21, &pwmcfg);
  crcStart(&CRCD1, &crc32_config);
  sdStart(&LPSD1, NULL);

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
