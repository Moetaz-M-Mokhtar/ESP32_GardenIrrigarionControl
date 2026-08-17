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
static bool isDowActive(uint8_t dow, uint8_t rtcDayOfWeek);
static void Scheduler_updateAlarmStatus(void);
static void Scheduler_loadNextEvent(void);
static void Scheduler_updateTaskCompleteAlarm(void);

/****************************** local function definition *****************************/
static bool isDowActive(uint8_t dow, uint8_t rtcDayOfWeek)
{
    return (dow >> (7 - rtcDayOfWeek)) & 0x01;
}
static void Scheduler_loadNextEvent(void)
{
    uint32_t minTimeNextEvent = 0xFFFFFFFF;
    uint32_t tempTimeOfNextEventAlarm;
    uint8_t alarmCount = sizeof(ScheduleAlarm_arr) / sizeof(ScheduleAlarm_arr[0]);
    uint8_t tempReturnValue = 0;
    bool isTimeValid = false;

    
    for(uint8_t loopIndex = 0; loopIndex < alarmCount; loopIndex++)
    {
        tempReturnValue = ScheduleAlarm_arr[loopIndex].nextTriggerTime(&tempTimeOfNextEventAlarm);
        
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
    uint8_t alarmCount = sizeof(ScheduleAlarm_arr) / sizeof(ScheduleAlarm_arr[0]);

    for(uint8_t loopIndex = 0; loopIndex < alarmCount; loopIndex++)
    {
        ScheduleAlarm_arr[loopIndex].evaluateAlarmState();
    }
}

static void Scheduler_updateTaskCompleteAlarm(void)
{
    uint32_t minTaskCompleteTime = 0xFFFFFFFF;
    uint32_t tempTaskCompleteTime;
    uint8_t alarmCount = sizeof(ScheduleAlarm_arr) / sizeof(ScheduleAlarm_arr[0]);
    uint8_t tempReturnValue = 0;
    bool isTimeValid = false;
    for(uint8_t loopIndex = 0; loopIndex < alarmCount; loopIndex++)
    {
        tempReturnValue = ScheduleAlarm_arr[loopIndex].taskCompleteTime(&tempTaskCompleteTime);
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

/****************************** global function definition ****************************/
bool ScheduleAlarm::nextTriggerTime(uint32_t* unix_time)
{
    bool OpStatus = false;
    DateTime currentTime = ClockDrift_getCorrectedTime();
    DateTime AlarmTime = DateTime(currentTime.year(), \
                                        currentTime.month(), \
                                        currentTime.day(), \
                                        this->hours, \
                                        this->minutes);
    *unix_time = 0xFFFFFFFF;

    if(this->dow != 0)
    {
        if(isDowActive(this->dow, AlarmTime.dayOfTheWeek()) &&
            (AlarmTime > currentTime))
        {
            *unix_time = AlarmTime.unixtime();
            OpStatus = true;
        }
        else
        {
            TimeSpan daySpan(1, 0, 0, 0);
            for(uint8_t loopIndex = 0; loopIndex < 7; loopIndex++)
            {
                AlarmTime = AlarmTime + daySpan;
                if(isDowActive(this->dow, AlarmTime.dayOfTheWeek()))
                {
                    *unix_time = AlarmTime.unixtime();
                    OpStatus = true;
                    break;
                }
            }
        }
    }
    return OpStatus;
}

bool ScheduleAlarm::taskCompleteTime(uint32_t* unix_time)
{
    bool OpStatus = false;

    if(this->dow != 0)
    {
        DateTime currentTime = ClockDrift_getCorrectedTime();

        if (!isDowActive(this->dow, currentTime.dayOfTheWeek()))
        {
            return false;
        }

        DateTime alarmStartTime = DateTime(currentTime.year(), \
                                            currentTime.month(), \
                                            currentTime.day(), \
                                            this->hours, \
                                            this->minutes);
        DateTime alarmEndTime = alarmStartTime + TimeSpan(this->period * 60);
        bool isActive = (currentTime >= alarmStartTime) && (currentTime < alarmEndTime);

        if (isActive)
        {
            *unix_time = alarmEndTime.unixtime();
            OpStatus = true;
        }
    }

    return OpStatus;   
}

void ScheduleAlarm::evaluateAlarmState(void)
{
    if (this->dow == 0)
    {
        return;
    }

    DateTime currentTime = ClockDrift_getCorrectedTime();

    if (!isDowActive(this->dow, currentTime.dayOfTheWeek()))
    {
        return;
    }

    DateTime alarmStartTime = DateTime(currentTime.year(), \
                                       currentTime.month(), \
                                       currentTime.day(), \
                                       this->hours, \
                                       this->minutes);
    DateTime alarmEndTime = alarmStartTime + TimeSpan(this->period * 60);
    bool isActive = (currentTime >= alarmStartTime) && (currentTime < alarmEndTime);

    if (!isActive)
    {
        return;
    }

    /* apply to each zone in the bitmask — only turn ON, never OFF.
       OFF is handled by Scheduler turning all non-forced drivers OFF first. */
    for (uint8_t i = 0; i < CFGM_MAX_DRIVERS; i++)
    {
        if ((this->zones & (1 << i)) == 0)
        {
            continue;
        }

        HW_Driver* driver = &HW_Driver_arr[i];

        if (driver->forced)
        {
            driver->set_HwState(driver->forcedState);
        }
        else
        {
            BleComm_setDriverTimer(i, alarmStartTime.unixtime(), this->period * 60);
            driver->set_HwState(HIGH);
        }
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
    uint8_t alarmCount = sizeof(ScheduleAlarm_arr) / sizeof(ScheduleAlarm_arr[0]);
    uint8_t tempReturnValue = 0;
    bool isTimeValid = false;

    for(uint8_t loopIndex = 0; loopIndex < alarmCount; loopIndex++)
    {
        tempReturnValue = ScheduleAlarm_arr[loopIndex].nextTriggerTime(&tempTimeOfNextEventAlarm);
        
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

bool Scheduler_IsDriverScheduledOn(uint8_t driverId)
{
    if (driverId >= CFGM_MAX_DRIVERS)
    {
        return false;
    }

    DateTime currentTime = ClockDrift_getCorrectedTime();

    for (uint8_t i = 0; i < CFGM_MAX_ALARMS; i++)
    {
        ScheduleAlarm* alarm = &ScheduleAlarm_arr[i];

        if (alarm->getDow() == 0)
        {
            continue;
        }

        /* check if this alarm controls the driver via zone bitmask */
        if ((alarm->getZones() & (1 << driverId)) == 0)
        {
            continue;
        }

        /* check day-of-week */
        if (!isDowActive(alarm->getDow(), currentTime.dayOfTheWeek()))
        {
            continue;
        }

        /* check time window */
        DateTime alarmStart = DateTime(currentTime.year(),
                                       currentTime.month(),
                                       currentTime.day(),
                                       alarm->getHours(),
                                       alarm->getMinutes());
        DateTime alarmEnd = alarmStart + TimeSpan(alarm->getPeriod() * 60);

        if ((currentTime >= alarmStart) && (currentTime < alarmEnd))
        {
            return true;
        }
    }

    return false;
}


