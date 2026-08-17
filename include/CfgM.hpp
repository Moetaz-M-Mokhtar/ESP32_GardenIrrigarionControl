#ifndef _CFGM_HPP_
#define _CFGM_HPP_

#include <stdint.h>
#include <RTClib.h>

/*********************Cfg types definition*********************/
typedef enum
{
    GPIO_DRIVE = 0,
    LATCH_SN7475N_DRIVE,
    LATCH_SN7475N_DECODER
} driveType_dt;

class HW_Driver
{
    public:
    gpio_num_t GPIO_Drive_pinNum;
    driveType_dt Solenoid_DriveType;
    gpio_num_t GPIO_Enable_pinNum;
    volatile uint8_t pin_OutputLevel;
    uint8_t coupled_HW_Driver_Idx;
    volatile bool forced;
    volatile uint8_t forcedState;
    HW_Driver(gpio_num_t GPIO_Drive_pinNum, \
                  driveType_dt Solenoid_DriveType, \
                  gpio_num_t GPIO_Enable_pinNum, \
                  uint8_t coupled_HW_Driver_Idx);
    bool set_HwState(uint8_t state);
};

class ScheduleAlarm
{
    private:
    uint8_t hours;
    uint8_t minutes;
    uint16_t period;
    uint8_t dow;
    uint8_t zones;      /* bitmask: bit0=zone0, bit1=zone1, etc. */
    HW_Driver* HW_Driver_Data;
    public:
    ScheduleAlarm(uint8_t h, uint8_t m, uint16_t period, uint8_t dow, uint8_t zones, HW_Driver* HW_Driver_Data);
    ~ScheduleAlarm(void);
    bool nextTriggerTime(uint32_t*);
    bool taskCompleteTime(uint32_t*);
    void evaluateAlarmState(void);

    /* getters */
    uint8_t getHours(void) const { return hours; }
    uint8_t getMinutes(void) const { return minutes; }
    uint16_t getPeriod(void) const { return period; }
    uint8_t getDow(void) const { return dow; }
    uint8_t getZones(void) const { return zones; }
    HW_Driver* getHwDriver(void) const { return HW_Driver_Data; }

    /* setters */
    void setHours(uint8_t h) { hours = h; }
    void setMinutes(uint8_t m) { minutes = m; }
    void setPeriod(uint16_t p) { period = p; }
    void setDow(uint8_t d) { dow = d; }
    void setZones(uint8_t z) { zones = z; }
    void setHwDriver(HW_Driver* d) { HW_Driver_Data = d; }
};
/*********************Global function declarations*********************/
#define CFGM_MAX_DRIVERS 4
#define CFGM_MAX_ALARMS  15

extern HW_Driver HW_Driver_arr[CFGM_MAX_DRIVERS];
extern ScheduleAlarm ScheduleAlarm_arr[CFGM_MAX_ALARMS];

void CfgM_Init(void);
bool CfgM_SetAlarm(uint8_t alarmId, uint8_t h, uint8_t m, uint16_t period, uint8_t dow, uint8_t zones);
bool CfgM_SetAlarmDriver(uint8_t alarmId, uint8_t driverId);
void CfgM_SaveToNvs(void);
void CfgM_LoadFromNvs(void);
bool CfgM_IsDriverEnabled(uint8_t driverId);
bool CfgM_SetDriverEnabled(uint8_t driverId, bool enabled);
void CfgM_SetPairOnNextWake(bool enable);
void CfgM_SetPaired(void);
void CfgM_ClearPaired(void);
void CfgM_MainFunction(void);

#endif /* _CFGM_HPP_ */