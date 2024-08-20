/* include files */
#include<Scheduler.hpp>
#include<CfgM.hpp>
#include<TimerCtrl.hpp>
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
/******************************* local function declaration *****************************/
static void Scheduler_updateAlarmStatus(void);
static void Scheduler_loadNextEvent(void);

/****************************** local function definition *****************************/
static void Scheduler_loadNextEvent(void)
{
    uint32_t minTimeNextEvent = 0xFFFFFFFF;
    uint32_t tempTimeOfNextEventAlarm = 0xFFFFFFFF;
    uint8_t alarmCount = sizeof(alarmsList) / sizeof(alarmsList[0]);
    uint8_t nextEventIdx = 0;
    bool isTimeValid = false;
    
    for(uint8_t loopIndex = 0; loopIndex < alarmCount; loopIndex++)
    {
        if(alarmsList[loopIndex].isEventActive() == true)
        {
            isTimeValid = true;
            tempTimeOfNextEventAlarm = alarmsList[loopIndex].timeToNextTrigger();
            
            if (tempTimeOfNextEventAlarm < minTimeNextEvent)
            {
                minTimeNextEvent = tempTimeOfNextEventAlarm;
                nextEventIdx = loopIndex;
            }
        }
    }
    
    if(isTimeValid == true)
    {
        TimerCtrl_setAlarm(TIMER1_INDEX, DateTime(SECONDS_FROM_1970_TO_2000 + minTimeNextEvent));
    }
}

static void Scheduler_updateAlarmStatus(void)
{
    DateTime currentTime = TimerCtrl_getCurrentTime();
    uint8_t alarmCount = sizeof(alarmsList) / sizeof(alarmsList[0]);
    DateTime alarmTime;

    for(uint8_t loopIndex = 0; loopIndex < alarmCount; loopIndex++)
    {
        alarmsList[loopIndex].evaluateEventAction(currentTime);
    }
}

/****************************** global function declaration ****************************/
uint32_t Scheduler_Alarm::timeToNextTrigger(void)
{
    uint32_t returnValue = 0xFFFFFFFF;
    DateTime currentTime = TimerCtrl_getCurrentTime();
    DateTime AlarmTime = DateTime(currentTime.year(), \
                                        currentTime.month(), \
                                        currentTime.day(), \
                                        this->hours, \
                                        this->minutes);

    if(((this->dow >> (7 - AlarmTime.dayOfTheWeek()) & 0x01) == 1) &&
        (AlarmTime > currentTime))
    {
        returnValue = AlarmTime.secondstime() - currentTime.secondstime();
    }
    else
    {
        for(uint8_t loopIndex = 0; loopIndex < 7; loopIndex++)
        {
            AlarmTime = AlarmTime + *(new TimeSpan(1,0,0,0));
            if((this->dow >> (7 - AlarmTime.dayOfTheWeek()) & 0x01) == 1)
            {
                returnValue = AlarmTime.secondstime() - currentTime.secondstime();
                break;
            }
        }
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

    if((this->isEventActive() == true) && \
       ((currentTime > alarmStartTime) & (currentTime < alarmEndTime)))
    {
        // set alarm action to active
    }
    else
    {
        // set alarm action to passive
    }

}

void Scheduler_Init(void)
{

}


