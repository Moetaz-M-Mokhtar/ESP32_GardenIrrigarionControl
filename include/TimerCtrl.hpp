#ifndef _TIMERCTRL_HPP_
#define _TIMERCTRL_HPP_

#include <RTClib.h>

#define TIMER1_INDEX        1
#define TIMER2_INDEX        2

bool TimerCtrl_Init(void);
void TimerCtrl_mainFunction();
bool TimerCtrl_resetAlarm(uint8_t alarmIndex);
DateTime TimerCtrl_getCurrentTime();
bool TimerCtrl_setAlarm(uint8_t alarmIndex, DateTime time);

#endif /* _TIMERCTRL_HPP_ */