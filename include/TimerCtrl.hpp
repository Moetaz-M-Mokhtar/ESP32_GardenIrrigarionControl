#ifndef _TIMERCTRL_HPP_
#define _TIMERCTRL_HPP_

#include <RTClib.h>

extern bool TimerCtrl_Init(void);
extern void TimerCtrl_mainFunction();
extern bool TimerCtrl_resetAlarm(uint8_t alarmIndex);
extern DateTime TimerCtrl_getCurrentTime();

#endif /* _TIMERCTRL_HPP_ */