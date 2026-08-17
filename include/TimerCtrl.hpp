#ifndef _TIMERCTRL_HPP_
#define _TIMERCTRL_HPP_

#include <RTClib.h>

#define TIMER1_INDEX        1
#define TIMER2_INDEX        2

bool TimerCtrl_Init(void);
void TimerCtrl_mainFunction(void);
bool TimerCtrl_resetAlarm(uint8_t alarmIndex);
DateTime TimerCtrl_getCurrentTime(void);
bool TimerCtrl_setAlarm(uint8_t alarmIndex, DateTime time);
bool TimerCtrl_adjustTime(DateTime newTime);
float TimerCtrl_getTemperature(void);

/* RTC alarm readback — for DebugM display */
DateTime TimerCtrl_getAlarmTime(uint8_t alarmIndex);

#endif /* _TIMERCTRL_HPP_ */
