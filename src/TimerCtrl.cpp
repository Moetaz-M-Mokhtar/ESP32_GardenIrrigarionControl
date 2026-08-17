/* include files */
#include <TimerCtrl.hpp>
#include <stdint.h>
#include <ErrM.hpp>
#include <Common.hpp>
#include <Preferences.h>

/****************************** local variable declaration *****************************/
static RTC_DS3231 DS3231Handler;
static DateTime DS3231CurrentTime = DateTime(F(__DATE__), F(__TIME__));

/****************************** global function definition ****************************/
bool TimerCtrl_Init(void)
{
    bool OpStatus = true;
    
    if (! DS3231Handler.begin())
    {
        OpStatus = false;
        ErrM_SetErrorStatus(ERRM_RTC_NOT_CONNECTED, true);
    }
    else
    {
        ErrM_SetErrorStatus(ERRM_RTC_NOT_CONNECTED, false);

        static Preferences prefs;
        prefs.begin("clock_init", false);

        if ((!prefs.getBool("clock_init", false)) || \
             (DS3231Handler.lostPower()))
        {
            if (DS3231Handler.lostPower())
            {
                ErrM_SetErrorStatus(ERRM_RTC_LOST_POWER, true);
            }
            DS3231Handler.adjust(DS3231CurrentTime);
        }
        else
        {
            DS3231CurrentTime = DS3231Handler.now();
        }

        prefs.putBool("clock_init", true);
            
        DS3231Handler.disable32K();
        DS3231Handler.clearAlarm(1);
        DS3231Handler.clearAlarm(2);
        DS3231Handler.writeSqwPinMode(DS3231_OFF);
        DS3231Handler.disableAlarm(1);
        DS3231Handler.disableAlarm(2);
    }
    return OpStatus;
}

void TimerCtrl_mainFunction(void)
{
    if(ErrM_GetFunctionPermission(ERRM_FUNC_TIMERCTRL) == true)
    {
        DateTime reading = DS3231Handler.now();

        if (reading.year() < COMMON_RTC_I2C_YEAR_MIN)
        {
            ErrM_SetErrorStatus(ERRM_RTC_I2C_FAILED, true);
            return;
        }
        ErrM_SetErrorStatus(ERRM_RTC_I2C_FAILED, false);

        DS3231CurrentTime = reading;
    }
}

DateTime TimerCtrl_getCurrentTime(void)
{
    return DS3231CurrentTime;
}

bool TimerCtrl_setAlarm(uint8_t alarmIndex, DateTime time)
{
    bool OpStatus = true;
    
    if(ErrM_GetFunctionPermission(ERRM_FUNC_TIMERCTRL) == true)
    {
        if(alarmIndex == TIMER1_INDEX)
        {
            DS3231Handler.setAlarm1(time, DS3231_A1_Hour);
        }
        else if(alarmIndex == TIMER2_INDEX)
        {
            DS3231Handler.setAlarm2(time, DS3231_A2_Hour);
        }
        else
        {
            OpStatus = false;
        }
    }
    else
    {
        OpStatus = false;
    }

    return OpStatus;
}

bool TimerCtrl_resetAlarm(uint8_t alarmIndex)
{
    bool OpStatus = true;

    if(ErrM_GetFunctionPermission(ERRM_FUNC_TIMERCTRL) == true)
    {
        if(alarmIndex == TIMER1_INDEX || alarmIndex == TIMER2_INDEX)
        {
            DS3231Handler.clearAlarm(alarmIndex);
            DS3231Handler.disableAlarm(alarmIndex);
        }
        else
        {
            OpStatus = false;
        }
    }
    else
    {
        OpStatus = false;
    }
    
    return OpStatus;
}

bool TimerCtrl_adjustTime(DateTime newTime)
{
    bool OpStatus = true;

    if(ErrM_GetFunctionPermission(ERRM_FUNC_TIMERCTRL) == true)
    {
        DS3231Handler.adjust(newTime);

        DateTime verify = DS3231Handler.now();
        if (verify.unixtime() != newTime.unixtime())
        {
            ErrM_SetErrorStatus(ERRM_RTC_I2C_FAILED, true);
            OpStatus = false;
        }
        else
        {
            ErrM_SetErrorStatus(ERRM_RTC_I2C_FAILED, false);
            DS3231CurrentTime = newTime;
        }
    }
    else
    {
        OpStatus = false;
    }

    return OpStatus;
}

float TimerCtrl_getTemperature(void)
{
    if (ErrM_GetFunctionPermission(ERRM_FUNC_TIMERCTRL))
    {
        return DS3231Handler.getTemperature();
    }
    return 0.0f;
}

DateTime TimerCtrl_getAlarmTime(uint8_t alarmIndex)
{
    if (alarmIndex == TIMER1_INDEX)
    {
        return DS3231Handler.getAlarm1();
    }
    else if (alarmIndex == TIMER2_INDEX)
    {
        return DS3231Handler.getAlarm2();
    }
    return DateTime((uint32_t)0);
}
