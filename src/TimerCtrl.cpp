/* include files */
#include <TimerCtrl.hpp>
#include <stdint.h>
#include <ErrM.hpp>
#include <Preferences.h>
/**************************************** define ***************************************/

/********************************* local type definition *******************************/

/****************************** local variable declaration *****************************/
static RTC_DS3231 DS3231Handler;
Preferences prefs;
static DateTime DS3231CurrentTime = DateTime(F(__DATE__), F(__TIME__));
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
/******************************* local function declaration *****************************/

/****************************** local function definition *****************************/
static void TimerCtrl_printDateTime(const DateTime& dt)
{
    Serial.print(dt.year(), DEC);
    Serial.print('/');
    Serial.print(dt.month(), DEC);
    Serial.print('/');
    Serial.print(dt.day(), DEC);
    Serial.print(" (");
    Serial.print(daysOfTheWeek[dt.dayOfTheWeek()]);
    Serial.print(") ");
    Serial.print(dt.hour(), DEC);
    Serial.print(':');
    Serial.print(dt.minute(), DEC);
    Serial.print(':');
    Serial.print(dt.second(), DEC);
    Serial.println();
}

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

static DateTime lastPrintTime = DateTime();

void TimerCtrl_mainFunction(void)
{
    if(ErrM_GetFunctionPermission(ERRM_FUNC_TIMERCTRL) == true)
    {
        DateTime reading = DS3231Handler.now();

        /* detect I2C failure — year 2000 means BCD registers read as 0x00 */
        if (reading.year() < 2024)
        {
            ErrM_SetErrorStatus(ERRM_RTC_I2C_FAILED, true);
            return;
        }
        ErrM_SetErrorStatus(ERRM_RTC_I2C_FAILED, false);

        DS3231CurrentTime = reading;

        if ((DS3231CurrentTime.unixtime() - lastPrintTime.unixtime()) >= 5)
        {
            TimerCtrl_printDateTime(DS3231CurrentTime);
            Serial.print("Temperature: ");
            Serial.print(DS3231Handler.getTemperature());
            Serial.println(" C");
            lastPrintTime = DS3231CurrentTime;
        }
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
        if(alarmIndex == 1)
        {
            DS3231Handler.setAlarm1(time, DS3231_A1_Hour);
            Serial.print("Alarm 1: ");
        }
        else if(alarmIndex == 2)
        {
            DS3231Handler.setAlarm2(time, DS3231_A2_Hour);
            Serial.print("Alarm 2: ");
        }
        else
        {
            OpStatus = false;
        }
        if (OpStatus)
        {
            TimerCtrl_printDateTime(time);
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

bool TimerCtrl_adjustTime(DateTime newTime)
{
    bool OpStatus = true;

    if(ErrM_GetFunctionPermission(ERRM_FUNC_TIMERCTRL) == true)
    {
        DS3231Handler.adjust(newTime);

        /* read back to verify I2C write succeeded */
        DateTime verify = DS3231Handler.now();
        if (verify.unixtime() != newTime.unixtime())
        {
            Serial.printf("TimerCtrl: I2C adjust verify failed (wrote %lu, read back %lu)\n",
                           newTime.unixtime(), verify.unixtime());
            ErrM_SetErrorStatus(ERRM_RTC_I2C_FAILED, true);
            OpStatus = false;
        }
        else
        {
            ErrM_SetErrorStatus(ERRM_RTC_I2C_FAILED, false);
            DS3231CurrentTime = newTime;
        }

        Serial.print("Time adjusted to: ");
        TimerCtrl_printDateTime(newTime);
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