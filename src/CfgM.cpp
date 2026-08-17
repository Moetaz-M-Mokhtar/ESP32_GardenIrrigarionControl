/* include files */
#include <CfgM.hpp>
#include <ErrM.hpp>
#include <TimerCtrl.hpp>
#include <ClockDrift.hpp>
#include <Preferences.h>
#include <BleComm.hpp>
/**************************************** define ***************************************/
#define NVS_NAMESPACE "cfgm"
#define NVS_KEY_ALARM_INITIALIZED "alarm_init"
#define NVS_KEY_DRIVER_INITIALIZED "drv_init"
#define NVS_KEY_DRIVER_ENABLED "drv_en"
#define NVS_KEY_ALARM_HOURS       "alarm_h"
#define NVS_KEY_ALARM_MINUTES     "alarm_m"
#define NVS_KEY_ALARM_PERIOD      "alarm_p"
#define NVS_KEY_ALARM_DOW         "alarm_d"
#define NVS_KEY_PAIR_ON_NEXT_WAKE "pair_nw"

/********************************* local type definition *******************************/

/****************************** local variable definition *****************************/
static volatile bool driverEnabled[CFGM_MAX_DRIVERS] = {true, true, true, true};
static Preferences nvsPrefs;
static volatile bool nvsDirty = false;

/****************************** local function declaration *****************************/

/****************************** global variable definition *****************************/
HW_Driver_cfg HW_Driver_cfg_arr[] =
{
    HW_Driver_cfg(
        GPIO_NUM_13,                    //GPIO_Drive_pinNum
        LATCH_SN7475N_DRIVE,            //Solenoid_DriveType
        GPIO_NUM_26,                    //GPIO_Enable_pinNum
        1                               //coupled_HW_Driver_Idx
    ),
    HW_Driver_cfg(
        GPIO_NUM_15,                    //GPIO_Drive_pinNum
        LATCH_SN7475N_DRIVE,            //Solenoid_DriveType
        GPIO_NUM_26,                    //GPIO_Enable_pinNum
        0                               //coupled_HW_Driver_Idx
    ),
    HW_Driver_cfg(
        GPIO_NUM_13,                    //GPIO_Drive_pinNum
        LATCH_SN7475N_DRIVE,            //Solenoid_DriveType
        GPIO_NUM_27,                    //GPIO_Enable_pinNum
        3                               //coupled_HW_Driver_Idx
    ),
    HW_Driver_cfg(
        GPIO_NUM_15,                    //GPIO_Drive_pinNum
        LATCH_SN7475N_DRIVE,            //Solenoid_DriveType
        GPIO_NUM_27,                    //GPIO_Enable_pinNum
        2                               //coupled_HW_Driver_Idx
    ),
};

ScheduleAlarm_cfg ScheduleAlarm_cfg_arr[] =
{
    ScheduleAlarm_cfg(0, 0, 30, 0, 0, &HW_Driver_cfg_arr[0]),
    ScheduleAlarm_cfg(0, 0, 30, 0, 0, &HW_Driver_cfg_arr[0]),
    ScheduleAlarm_cfg(0, 0, 30, 0, 0, &HW_Driver_cfg_arr[0]),
    ScheduleAlarm_cfg(0, 0, 30, 0, 0, &HW_Driver_cfg_arr[0]),
    ScheduleAlarm_cfg(0, 0, 30, 0, 0, &HW_Driver_cfg_arr[0]),
    ScheduleAlarm_cfg(0, 0, 30, 0, 0, &HW_Driver_cfg_arr[0]),
    ScheduleAlarm_cfg(0, 0, 30, 0, 0, &HW_Driver_cfg_arr[0]),
    ScheduleAlarm_cfg(0, 0, 30, 0, 0, &HW_Driver_cfg_arr[0]),
    ScheduleAlarm_cfg(0, 0, 30, 0, 0, &HW_Driver_cfg_arr[0]),
    ScheduleAlarm_cfg(0, 0, 30, 0, 0, &HW_Driver_cfg_arr[0]),
    ScheduleAlarm_cfg(0, 0, 30, 0, 0, &HW_Driver_cfg_arr[0]),
    ScheduleAlarm_cfg(0, 0, 30, 0, 0, &HW_Driver_cfg_arr[0]),
    ScheduleAlarm_cfg(0, 0, 30, 0, 0, &HW_Driver_cfg_arr[0]),
    ScheduleAlarm_cfg(0, 0, 30, 0, 0, &HW_Driver_cfg_arr[0]),
    ScheduleAlarm_cfg(0, 0, 30, 0, 0, &HW_Driver_cfg_arr[0])
};

/******************************* local function definition *****************************/
static bool isDowActive(uint8_t dow, uint8_t rtcDayOfWeek)
{
    return (dow >> (7 - rtcDayOfWeek)) & 0x01;
}

/****************************** global function definition ****************************/
HW_Driver_cfg::HW_Driver_cfg(gpio_num_t GPIO_Drive_pinNum, driveType_dt Solenoid_DriveType, gpio_num_t GPIO_Enable_pinNum, uint8_t coupled_HW_Driver_Idx)
                        :GPIO_Drive_pinNum(GPIO_Drive_pinNum), Solenoid_DriveType(Solenoid_DriveType), GPIO_Enable_pinNum(GPIO_Enable_pinNum), coupled_HW_Driver_Idx(coupled_HW_Driver_Idx)
{
    this->pin_OutputLevel = LOW;
    this->forced = false;
    this->forcedState = LOW;
}

bool HW_Driver_cfg::set_HwState(uint8_t state)
{
    bool OpStatus = true; 
    if ((state != LOW) && (state != HIGH))
    {
        OpStatus = false;
    }
    else
    {
        this->pin_OutputLevel = state;
    }
    return OpStatus;
}

ScheduleAlarm_cfg::ScheduleAlarm_cfg(uint8_t h, uint8_t m, uint16_t period, uint8_t dow, uint8_t zones, HW_Driver_cfg* HW_Driver_Data)
                                :hours(h), minutes(m), period(period), dow(dow), zones(zones), HW_Driver_Data(HW_Driver_Data)
{

}

ScheduleAlarm_cfg::~ScheduleAlarm_cfg()
{
}

bool ScheduleAlarm_cfg::nextTriggerTime(uint32_t* unix_time)
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

bool ScheduleAlarm_cfg::taskCompleteTime(uint32_t* unix_time)
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

void ScheduleAlarm_cfg::evaluateAlarmState(void)
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

        HW_Driver_cfg* driver = &HW_Driver_cfg_arr[i];

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

void CfgM_Init(void)
{
    nvsPrefs.begin(NVS_NAMESPACE, false);
    CfgM_LoadFromNvs();
}

void CfgM_SaveToNvs(void)
{
    nvsPrefs.putBool(NVS_KEY_ALARM_INITIALIZED, true);
    nvsPrefs.putBool(NVS_KEY_DRIVER_INITIALIZED, true);

    for (uint8_t i = 0; i < CFGM_MAX_ALARMS; i++)
    {
        String prefix = "a" + String(i) + "_";
        nvsPrefs.putUChar((prefix + "h").c_str(), ScheduleAlarm_cfg_arr[i].getHours());
        nvsPrefs.putUChar((prefix + "m").c_str(), ScheduleAlarm_cfg_arr[i].getMinutes());
        nvsPrefs.putUShort((prefix + "p").c_str(), ScheduleAlarm_cfg_arr[i].getPeriod());
        nvsPrefs.putUChar((prefix + "d").c_str(), ScheduleAlarm_cfg_arr[i].getDow());
        nvsPrefs.putUChar((prefix + "z").c_str(), ScheduleAlarm_cfg_arr[i].getZones());

        /* save driver mapping */
        HW_Driver_cfg* hw = ScheduleAlarm_cfg_arr[i].getHwDriver();
        uint8_t driverIdx = 0;
        for (uint8_t d = 0; d < CFGM_MAX_DRIVERS; d++)
        {
            if (hw == &HW_Driver_cfg_arr[d]) { driverIdx = d; break; }
        }
        nvsPrefs.putUChar((prefix + "drv").c_str(), driverIdx);
    }

    for (uint8_t i = 0; i < CFGM_MAX_DRIVERS; i++)
    {
        nvsPrefs.putBool((String(NVS_KEY_DRIVER_ENABLED) + String(i)).c_str(), driverEnabled[i]);
    }

    Serial.println("CfgM: Configuration saved to NVS");
}

void CfgM_LoadFromNvs(void)
{
    /* Load driver enabled state */
    if (nvsPrefs.getBool(NVS_KEY_DRIVER_INITIALIZED, false))
    {
        for (uint8_t i = 0; i < CFGM_MAX_DRIVERS; i++)
        {
            driverEnabled[i] = nvsPrefs.getBool((String(NVS_KEY_DRIVER_ENABLED) + String(i)).c_str(), true);
            Serial.printf("CfgM: Driver %d %s\n", i, driverEnabled[i] ? "enabled" : "disabled");
        }
    }

    /* Load alarm configuration */
    if (!nvsPrefs.getBool(NVS_KEY_ALARM_INITIALIZED, false))
    {
        Serial.println("CfgM: No saved alarms, using defaults");
        return;
    }

    for (uint8_t i = 0; i < CFGM_MAX_ALARMS; i++)
    {
        String prefix = "a" + String(i) + "_";
        uint8_t h = nvsPrefs.getUChar((prefix + "h").c_str(), ScheduleAlarm_cfg_arr[i].getHours());
        uint8_t m = nvsPrefs.getUChar((prefix + "m").c_str(), ScheduleAlarm_cfg_arr[i].getMinutes());
        uint16_t p = nvsPrefs.getUShort((prefix + "p").c_str(), ScheduleAlarm_cfg_arr[i].getPeriod());
        uint8_t d = nvsPrefs.getUChar((prefix + "d").c_str(), ScheduleAlarm_cfg_arr[i].getDow());
        uint8_t z = nvsPrefs.getUChar((prefix + "z").c_str(), ScheduleAlarm_cfg_arr[i].getZones());

        /* validate loaded values — NVS corruption can produce garbage */
        if (h < 24 && m < 60 && p >= 15)
        {
            ScheduleAlarm_cfg_arr[i].setHours(h);
            ScheduleAlarm_cfg_arr[i].setMinutes(m);
            ScheduleAlarm_cfg_arr[i].setPeriod(p);
            ScheduleAlarm_cfg_arr[i].setDow(d);
            ScheduleAlarm_cfg_arr[i].setZones(z);
        }
        else if (d == 0)
        {
            /* free slot — keep defaults, no validation needed */
        }
        else
        {
            /* corrupted — reset this slot to free */
            Serial.printf("CfgM: Alarm %d corrupted (h=%d m=%d p=%d d=0x%02X), resetting\n", i, h, m, p, d);
            ErrM_SetErrorStatus(ERRM_NVS_LOAD_CORRUPTED, true);
            ScheduleAlarm_cfg_arr[i].setDow(0);
        }

        /* restore driver mapping */
        uint8_t drv = nvsPrefs.getUChar((prefix + "drv").c_str(), 0xFF);
        if (drv < CFGM_MAX_DRIVERS)
        {
            ScheduleAlarm_cfg_arr[i].setHwDriver(&HW_Driver_cfg_arr[drv]);
            Serial.printf("CfgM: Alarm %d -> driver %d\n", i, drv);
        }

        Serial.printf("CfgM: Alarm %d loaded: %02d:%02d, period=%d, dow=0x%02X, zones=0x%02X\n",
                       i, h, m, p, d, z);
    }
}

bool CfgM_SetAlarm(uint8_t alarmId, uint8_t h, uint8_t m, uint16_t period, uint8_t dow, uint8_t zones)
{
    if (alarmId >= CFGM_MAX_ALARMS)
    {
        Serial.printf("CfgM: Invalid alarm ID %d\n", alarmId);
        return false;
    }

    if (h >= 24 || m >= 60 || period < 15)
    {
        Serial.printf("CfgM: Invalid alarm params h=%d m=%d period=%d dow=0x%02X\n", h, m, period, dow);
        return false;
    }

    ScheduleAlarm_cfg_arr[alarmId].setHours(h);
    ScheduleAlarm_cfg_arr[alarmId].setMinutes(m);
    ScheduleAlarm_cfg_arr[alarmId].setPeriod(period);
    ScheduleAlarm_cfg_arr[alarmId].setDow(dow);
    ScheduleAlarm_cfg_arr[alarmId].setZones(zones);

    /* defer NVS save to main loop — avoids blocking NimBLE task with flash writes */
    nvsDirty = true;

    Serial.printf("CfgM: Alarm %d set to %02d:%02d, period=%d, dow=0x%02X, zones=0x%02X\n",
                   alarmId, h, m, period, dow, zones);

    return true;
}

bool CfgM_SetAlarmDriver(uint8_t alarmId, uint8_t driverId)
{
    if (alarmId >= CFGM_MAX_ALARMS || driverId >= CFGM_MAX_DRIVERS)
    {
        Serial.printf("CfgM: Invalid alarm %d or driver %d\n", alarmId, driverId);
        return false;
    }

    ScheduleAlarm_cfg_arr[alarmId].setHwDriver(&HW_Driver_cfg_arr[driverId]);

    /* defer NVS save to main loop */
    nvsDirty = true;

    Serial.printf("CfgM: Alarm %d assigned to driver %d\n", alarmId, driverId);
    return true;
}

bool CfgM_IsDriverEnabled(uint8_t driverId)
{
    if (driverId >= CFGM_MAX_DRIVERS)
    {
        return false;
    }
    return driverEnabled[driverId];
}

bool CfgM_SetDriverEnabled(uint8_t driverId, bool enabled)
{
    if (driverId >= CFGM_MAX_DRIVERS)
    {
        Serial.printf("CfgM: Invalid driver ID %d\n", driverId);
        return false;
    }

    driverEnabled[driverId] = enabled;
    
    /* defer NVS save to main loop */
    nvsDirty = true;
    
    Serial.printf("CfgM: Driver %d %s\n", driverId, enabled ? "enabled" : "disabled");
    return true;
}

void CfgM_SetPairOnNextWake(bool enable)
{
    nvsPrefs.putBool(NVS_KEY_PAIR_ON_NEXT_WAKE, enable);
    Serial.printf("CfgM: Pair on next wake %s\n", enable ? "enabled" : "disabled");
}

void CfgM_SetPaired(void)
{
    nvsPrefs.putBool("paired", true);
    Serial.println("CfgM: Device marked as paired");
}

void CfgM_ClearPaired(void)
{
    nvsPrefs.putBool("paired", false);
    Serial.println("CfgM: Pairing cleared");
}

bool CfgM_IsDriverScheduledOn(uint8_t driverId)
{
    if (driverId >= CFGM_MAX_DRIVERS)
    {
        return false;
    }

    DateTime currentTime = ClockDrift_getCorrectedTime();

    for (uint8_t i = 0; i < CFGM_MAX_ALARMS; i++)
    {
        ScheduleAlarm_cfg* alarm = &ScheduleAlarm_cfg_arr[i];

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

void CfgM_MainFunction(void)
{
    if (nvsDirty)
    {
        nvsDirty = false;
        CfgM_SaveToNvs();
    }
}