/* include files */
#include <TimerCtrl.hpp>
#include <stdint.h>
#include <ErrM.hpp>
/**************************************** define ***************************************/

/********************************* local type definition *******************************/

/****************************** local variable declaration *****************************/
static RTC_DS3231 DS3231Handler;
static DateTime DS3231CurrentTime = DateTime(F(__DATE__), F(__TIME__));
/******************************* local function declaration *****************************/

/****************************** local function definition *****************************/

/****************************** global function declaration ****************************/
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

        if (DS3231Handler.lostPower())
        {
            ErrM_SetErrorStatus(ERRM_RTC_LOST_POWER, true);
            // When time needs to be set on a new device, or after a power loss, the
            // following line sets the RTC to the date & time this sketch was compiled
            DS3231Handler.adjust(DS3231CurrentTime);
            // This line sets the RTC with an explicit date & time, for example to set
            // January 21, 2014 at 3am you would call:
            // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
        }
        else
        {
            DS3231CurrentTime = DS3231Handler.now();
        }
            
        //we don't need the 32K Pin, so disable it
        DS3231Handler.disable32K();

        // set alarm 1, 2 flag to false (so alarm 1, 2 didn't happen so far)
        // if not done, this easily leads to problems, as both register aren't reset on reboot/recompile
        DS3231Handler.clearAlarm(1);
        DS3231Handler.clearAlarm(2);

        // stop oscillating signals at SQW Pin
        // otherwise setAlarm will fail
        DS3231Handler.writeSqwPinMode(DS3231_OFF);

        // turn off alarms (in case it isn't off already)
        // again, this isn't done at reboot, so a previously set alarm could easily go overlooked
        // alarms will be set based on configuration manager initialization
        DS3231Handler.disableAlarm(1);
        DS3231Handler.disableAlarm(2);
    }
    return OpStatus;
}

void TimerCtrl_mainFunction()
{
    if(ErrM_GetFunctionPermission(ERRM_FUNC_SPRINKLE) == true)
    {
        DS3231CurrentTime = DS3231Handler.now();
    }
}

DateTime TimerCtrl_getCurrentTime(void)
{
    return DS3231CurrentTime;
}

bool TimerCtrl_setAlarm(uint8_t alarmIndex, DateTime time)
{
    bool OpStatus = true;
    
    if(ErrM_GetFunctionPermission(ERRM_FUNC_SPRINKLE) == true)
    {
        if(alarmIndex == 1)
        {
            DS3231Handler.setAlarm1(time, DS3231_A1_Hour);
        }
        else if(alarmIndex == 2)
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

    if(ErrM_GetFunctionPermission(ERRM_FUNC_SPRINKLE) == true)
    {
        if(alarmIndex == 1 || alarmIndex == 2)
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