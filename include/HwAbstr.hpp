#ifndef _HWABSTR_HPP_
#define _HWABSTR_HPP_

#include <stdint.h>
#include <ErrM.hpp>

/*********************define*********************/
#define HWABSTR_PAIRING_BUTTON_PIN   GPIO_NUM_32   /* active LOW: press to enter pairing mode */
#define HWABSTR_PAIRING_TIMEOUT_SEC  30            /* 30 seconds pairing window */
#define HWABSTR_RTC_INTERRUPT_PIN    GPIO_NUM_4    /* connected to DS3231 SQW pin */

/*********************typeDef*********************/

/*********************global function declaration*********************/
void HwAbstr_Init(void);
void HwAbstr_MainFunction(void);
bool HwAbstr_isRtcWake(void);
void HwAbstr_GoToDeepSleep(uint32_t sleepSeconds);
int HwAbstr_GetBootCount(void);
void HwAbstr_WriteAllDriversOff(void);

#endif /* _HWABSTR_HPP_ */