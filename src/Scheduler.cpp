/* include files */
#include<Scheduler.hpp>
#include<CfgM.hpp>
#include<TimerCtrl.hpp>
#include<HwAbstr.hpp>
/**************************************** define ***************************************/

/********************************* local type definition *******************************/

/****************************** local variable declaration *****************************/
Scheduler_Alarm alarmsList[6] = 
{
    Scheduler_Alarm(7, 0, 20, 0xFA, 0, true),
    Scheduler_Alarm(7, 40, 20, 0xFA, 1, true),
    Scheduler_Alarm(8, 20, 20, 0xFA, 2, true),
    Scheduler_Alarm(19, 0, 20, 0xFE, 0, true),
    Scheduler_Alarm(19, 40, 20, 0xFE, 1, true),
    Scheduler_Alarm(20, 20, 20, 0xFE, 2, true)
};

SprinklerDt sprinklerList[3] = 
{
    { GPIO_NUM_16 }, // sprinkler 0
    { GPIO_NUM_17 }, // sprinkler 1
    { GPIO_NUM_18 } // sprinkler 2
};

/******************************* local function declaration *****************************/
static void Scheduler_updateAlarmStatus(void);
static void Scheduler_loadNextEvent(void);

/****************************** local function definition *****************************/
static void Scheduler_loadNextEvent(void)
{
    uint32_t minTimeNextEvent = 0xFFFFFFFF;
    uint32_t tempTimeOfNextEventAlarm;
    uint8_t alarmCount = sizeof(alarmsList) / sizeof(alarmsList[0]);
    uint8_t tempReturnValue = 0;
    bool isTimeValid = false;

    
    for(uint8_t loopIndex = 0; loopIndex < alarmCount; loopIndex++)
    {
        tempReturnValue = alarmsList[loopIndex].nextTriggerTime(&tempTimeOfNextEventAlarm);
        
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
    DateTime currentTime = TimerCtrl_getCurrentTime();
    uint8_t alarmCount = sizeof(alarmsList) / sizeof(alarmsList[0]);

    for(uint8_t loopIndex = 0; loopIndex < alarmCount; loopIndex++)
    {
        alarmsList[loopIndex].evaluateEventAction(currentTime);
    }
}

static void Scheduler_updateTaskCompleteAlarm(void)
{
    uint32_t minTaskCompleteTime = 0xFFFFFFFF;
    uint32_t tempTaskCompleteTime;
    uint8_t alarmCount = sizeof(alarmsList) / sizeof(alarmsList[0]);
    uint8_t tempReturnValue = 0;
    bool isTimeValid = false;
    for(uint8_t loopIndex = 0; loopIndex < alarmCount; loopIndex++)
    {
        tempReturnValue = alarmsList[loopIndex].taskCompleteTime(&tempTaskCompleteTime);
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
uint8_t Scheduler_Alarm::nextTriggerTime(uint32_t* triggerTime)
{
    uint8_t returnValue = 0;
    DateTime currentTime = TimerCtrl_getCurrentTime();
    DateTime AlarmTime = DateTime(currentTime.year(), \
                                        currentTime.month(), \
                                        currentTime.day(), \
                                        this->hours, \
                                        this->minutes);
    *triggerTime = 0xFFFFFFFF;

    if(this->getEventEnabled() == true)
    {
        if(((this->dow >> (7 - AlarmTime.dayOfTheWeek()) & 0x01) == 1) &&
            (AlarmTime > currentTime))
        {
            *triggerTime = AlarmTime.unixtime() - currentTime.unixtime();
            returnValue = 1;
        }
        else
        {
            for(uint8_t loopIndex = 0; loopIndex < 7; loopIndex++)
            {
                AlarmTime = AlarmTime + *(new TimeSpan(1,0,0,0));
                if((this->dow >> (7 - AlarmTime.dayOfTheWeek()) & 0x01) == 1)
                {
                    returnValue = 1;
                    *triggerTime = AlarmTime.unixtime() - currentTime.unixtime();
                    break;
                }
            }
        }
    }
    return returnValue;
}

uint8_t Scheduler_Alarm::taskCompleteTime(uint32_t* taskCompleteTime)
{
    uint8_t returnValue = 0;

    if(this->alarmState == 1)
    {
        DateTime currentTime = TimerCtrl_getCurrentTime();
        DateTime AlarmTime = DateTime(currentTime.year(), \
                                        currentTime.month(), \
                                        currentTime.day(), \
                                        this->hours, \
                                        this->minutes);
        *taskCompleteTime = (AlarmTime + TimeSpan(this->period * 60)).unixtime();
        returnValue = 1;
    }

    return returnValue;    
}

void Scheduler_Alarm::evaluateEventAction(DateTime currentTime)
{
    DateTime alarmStartTime = DateTime(currentTime.year(), \
                                        currentTime.month(), \
                                        currentTime.day(), \
                                        this->hours, \
                                        this->minutes);
    DateTime alarmEndTime = alarmStartTime + TimeSpan(this->period * 60);

    if((this->getEventEnabled() == true) && \
       ((currentTime > alarmStartTime) & (currentTime < alarmEndTime)))
    {
        this->alarmState = 1;
        HWAbstr_updateGPIOPinState(sprinklerList[this->sprinklerIdx].pinNumber, HIGH);
    }
    else
    {
        this->alarmState = 0;
        HWAbstr_updateGPIOPinState(sprinklerList[this->sprinklerIdx].pinNumber, LOW);
    }

}

Scheduler_Alarm::Scheduler_Alarm(uint8_t h, uint8_t m, uint8_t period, uint8_t dow, uint8_t sprinklerIdx, uint8_t isEnabled)
{
    this->hours = h;
    this->minutes = m;
    this->sprinklerIdx = sprinklerIdx;
    this->isEnabled = isEnabled;
    this->dow = dow;
    this->period = period;
    this->alarmState = 0;
    HWAbstr_updateGPIOPinState(sprinklerList[this->sprinklerIdx].pinNumber, LOW);
}

Scheduler_Alarm::~Scheduler_Alarm()
{
    HWAbstr_updateGPIOPinState(sprinklerList[this->sprinklerIdx].pinNumber, LOW);
}

void Scheduler_MainFunction(void)
{
    if(ErrM_GetFunctionPermission(ERRM_FUNC_TIMERCTRL) == true)
    {
        Scheduler_updateAlarmStatus();
        Scheduler_loadNextEvent();
        Scheduler_updateTaskCompleteAlarm();
    }
}


