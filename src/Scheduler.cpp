/* include files */
#include<Scheduler.hpp>
#include<CfgM.hpp>
#include<TimerCtrl.hpp>
#include<ClockDrift.hpp>
#include<ErrM.hpp>
#include<BleComm.hpp>
/**************************************** define ***************************************/

/********************************* local type definition *******************************/

/****************************** local variable declaration *****************************/
static uint8_t prevAlarmState[CFGM_MAX_DRIVERS] = {0, 0, 0, 0};

/******************************* local function declaration *****************************/
static void Scheduler_updateAlarmStatus(void);
static void Scheduler_loadNextEvent(void);
static void Scheduler_updateTaskCompleteAlarm(void);

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
        DateTime rawAlarm = ClockDrift_correctedToRaw(DateTime(minTimeNextEvent));
        TimerCtrl_setAlarm(TIMER1_INDEX, rawAlarm);
    }
    else
    {
        DateTime correctedFallback = ClockDrift_getCorrectedTime() + TimeSpan(SCHEDULER_FALLBACK_ALARM_SEC);
        TimerCtrl_setAlarm(TIMER1_INDEX, ClockDrift_correctedToRaw(correctedFallback));
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
        DateTime rawComplete = ClockDrift_correctedToRaw(DateTime(minTaskCompleteTime));
        TimerCtrl_setAlarm(TIMER2_INDEX, rawComplete);
    }
    else
    {
        TimerCtrl_resetAlarm(TIMER2_INDEX);
    }
}

/****************************** global function declaration ****************************/
void Scheduler_MainFunction(void)
{
    if(ErrM_GetFunctionPermission(ERRM_FUNC_SCHEDULER) == true &&
       ErrM_GetFunctionPermission(ERRM_FUNC_TIMERCTRL) == true)
    {
        Scheduler_updateAlarmStatus();
        Scheduler_loadNextEvent();
        Scheduler_updateTaskCompleteAlarm();
    }
}

uint32_t Scheduler_GetSecondsUntilNextAlarm(void)
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
        uint32_t now = ClockDrift_getCorrectedTime().unixtime();
        if (minTimeNextEvent > now)
        {
            return (minTimeNextEvent - now);
        }
        return 1; /* alarm is now or past */
    }
    return SCHEDULER_FALLBACK_SLEEP_SEC;
}


