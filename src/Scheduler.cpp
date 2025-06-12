/* include files */
#include<Scheduler.hpp>
#include<CfgM.hpp>
#include<TimerCtrl.hpp>
#include<HwAbstr.hpp>
/**************************************** define ***************************************/

/********************************* local type definition *******************************/

/****************************** local variable declaration *****************************/

/******************************* local function declaration *****************************/
static void Scheduler_updateAlarmStatus(void);
static void Scheduler_loadNextEvent(void);

/****************************** local function definition *****************************/
static void Scheduler_loadNextEvent(void)
{
    uint32_t minTimeNextEvent = 0xFFFFFFFF;
    uint32_t tempTimeOfNextEventAlarm;
    uint8_t alarmCount = sizeof(ScheduleAlarm_cfg_arr) / sizeof(ScheduleAlarm_cfg_arr[0]);
    uint8_t tempReturnValue = 0;
    bool isTimeValid = false;

    
    for(uint8_t loopIndex = 0; loopIndex < alarmCount; loopIndex++)
    {
        tempReturnValue = ScheduleAlarm_cfg_arr[loopIndex].nextTriggerTime(&tempTimeOfNextEventAlarm);
        
        if ((tempReturnValue == true) && \
            (tempTimeOfNextEventAlarm < minTimeNextEvent))
        {
            isTimeValid = true;
            minTimeNextEvent = tempTimeOfNextEventAlarm;
        }
    }
    
    if(isTimeValid == true)
    {
        TimerCtrl_setAlarm(TIMER1_INDEX, DateTime(minTimeNextEvent));
    }
    else
    {
        TimerCtrl_setAlarm(TIMER1_INDEX, TimerCtrl_getCurrentTime() + TimeSpan(180 * 60));
    }
}

static void Scheduler_updateAlarmStatus(void)
{
    uint8_t alarmCount = sizeof(ScheduleAlarm_cfg_arr) / sizeof(ScheduleAlarm_cfg_arr[0]);

    for(uint8_t loopIndex = 0; loopIndex < alarmCount; loopIndex++)
    {
        ScheduleAlarm_cfg_arr[loopIndex].evaluateAlarmState();
    }
}

static void Scheduler_updateTaskCompleteAlarm(void)
{
    uint32_t minTaskCompleteTime = 0xFFFFFFFF;
    uint32_t tempTaskCompleteTime;
    uint8_t alarmCount = sizeof(ScheduleAlarm_cfg_arr) / sizeof(ScheduleAlarm_cfg_arr[0]);
    uint8_t tempReturnValue = 0;
    bool isTimeValid = false;
    for(uint8_t loopIndex = 0; loopIndex < alarmCount; loopIndex++)
    {
        tempReturnValue = ScheduleAlarm_cfg_arr[loopIndex].taskCompleteTime(&tempTaskCompleteTime);
        if ((tempReturnValue == true) && \
            (tempTaskCompleteTime < minTaskCompleteTime))
        {
            isTimeValid = true;
            minTaskCompleteTime = tempTaskCompleteTime;
        }
    }

    if(isTimeValid == true)
    {
        TimerCtrl_setAlarm(TIMER2_INDEX, DateTime(minTaskCompleteTime));
    }
    else
    {
        TimerCtrl_resetAlarm(TIMER2_INDEX);
    }
}

/****************************** global function declaration ****************************/
void Scheduler_MainFunction(void)
{
    if(ErrM_GetFunctionPermission(ERRM_FUNC_TIMERCTRL) == true)
    {
        Scheduler_updateAlarmStatus();
        Scheduler_loadNextEvent();
        Scheduler_updateTaskCompleteAlarm();
    }
}


