#ifndef _CFGM_HPP_
#define _CFGM_HPP_

#include <stdint.h>
#include <HwAbstr.hpp>
#include <Scheduler.hpp>

/*********************Global function declarations*********************/
void CfgM_Init(void);
bool CfgM_SetAlarm(uint8_t alarmId, uint8_t h, uint8_t m, uint16_t period, uint8_t dow, uint8_t zones);
bool CfgM_SetAlarmDriver(uint8_t alarmId, uint8_t driverId);
void CfgM_SaveToNvs(void);
void CfgM_LoadFromNvs(void);
bool CfgM_IsDriverEnabled(uint8_t driverId);
bool CfgM_SetDriverEnabled(uint8_t driverId, bool enabled);

#endif /* _CFGM_HPP_ */
