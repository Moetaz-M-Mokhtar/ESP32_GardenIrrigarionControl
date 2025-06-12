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

class HW_Driver_cfg
{
    public:
    uint8_t GPIO_Drive_pinNum;
    driveType_dt Solenoid_DriveType;
    uint8_t GPIO_Enable_pinNum;
    uint8_t pin_OutputLevel;
    uint8_t coupled_HW_Driver_Idx;
    HW_Driver_cfg(uint8_t GPIO_Drive_pinNum, \
                  driveType_dt Solenoid_DriveType, \
                  uint8_t GPIO_Enable_pinNum, \
                  uint8_t coupled_HW_Driver_Idx);
    bool set_HwState(uint8_t state);
};

class ScheduleAlarm_cfg
{
    private:
    uint8_t hours;
    uint8_t minutes;
    uint8_t period;
    uint8_t dow;
    uint8_t isEnabled;
    HW_Driver_cfg* HW_Driver_Data;
    public:
    ScheduleAlarm_cfg(uint8_t h, uint8_t m, uint8_t period, uint8_t dow, uint8_t isEnabled, HW_Driver_cfg* HW_Driver_Data);
    ~ScheduleAlarm_cfg(void);
    bool nextTriggerTime(uint32_t*);
    bool taskCompleteTime(uint32_t*);
    void evaluateAlarmState(void);
};
/*********************Global variables*********************/
extern HW_Driver_cfg HW_Driver_cfg_arr[3];
extern ScheduleAlarm_cfg ScheduleAlarm_cfg_arr[6];
#endif /* _CFGM_HPP_ */