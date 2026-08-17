/* include files */
#include <CfgM.hpp>
#include <ErrM.hpp>
#include <Preferences.h>
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
HW_Driver HW_Driver_arr[] =
{
    HW_Driver(
        GPIO_NUM_13,                    //GPIO_Drive_pinNum
        LATCH_SN7475N_DRIVE,            //Solenoid_DriveType
        GPIO_NUM_26,                    //GPIO_Enable_pinNum
        1                               //coupled_HW_Driver_Idx
    ),
    HW_Driver(
        GPIO_NUM_15,                    //GPIO_Drive_pinNum
        LATCH_SN7475N_DRIVE,            //Solenoid_DriveType
        GPIO_NUM_26,                    //GPIO_Enable_pinNum
        0                               //coupled_HW_Driver_Idx
    ),
    HW_Driver(
        GPIO_NUM_13,                    //GPIO_Drive_pinNum
        LATCH_SN7475N_DRIVE,            //Solenoid_DriveType
        GPIO_NUM_27,                    //GPIO_Enable_pinNum
        3                               //coupled_HW_Driver_Idx
    ),
    HW_Driver(
        GPIO_NUM_15,                    //GPIO_Drive_pinNum
        LATCH_SN7475N_DRIVE,            //Solenoid_DriveType
        GPIO_NUM_27,                    //GPIO_Enable_pinNum
        2                               //coupled_HW_Driver_Idx
    ),
};

ScheduleAlarm ScheduleAlarm_arr[] =
{
    ScheduleAlarm(0, 0, 30, 0, 0, &HW_Driver_arr[0]),
    ScheduleAlarm(0, 0, 30, 0, 0, &HW_Driver_arr[0]),
    ScheduleAlarm(0, 0, 30, 0, 0, &HW_Driver_arr[0]),
    ScheduleAlarm(0, 0, 30, 0, 0, &HW_Driver_arr[0]),
    ScheduleAlarm(0, 0, 30, 0, 0, &HW_Driver_arr[0]),
    ScheduleAlarm(0, 0, 30, 0, 0, &HW_Driver_arr[0]),
    ScheduleAlarm(0, 0, 30, 0, 0, &HW_Driver_arr[0]),
    ScheduleAlarm(0, 0, 30, 0, 0, &HW_Driver_arr[0]),
    ScheduleAlarm(0, 0, 30, 0, 0, &HW_Driver_arr[0]),
    ScheduleAlarm(0, 0, 30, 0, 0, &HW_Driver_arr[0]),
    ScheduleAlarm(0, 0, 30, 0, 0, &HW_Driver_arr[0]),
    ScheduleAlarm(0, 0, 30, 0, 0, &HW_Driver_arr[0]),
    ScheduleAlarm(0, 0, 30, 0, 0, &HW_Driver_arr[0]),
    ScheduleAlarm(0, 0, 30, 0, 0, &HW_Driver_arr[0]),
    ScheduleAlarm(0, 0, 30, 0, 0, &HW_Driver_arr[0])
};

/****************************** global function definition ****************************/
HW_Driver::HW_Driver(gpio_num_t GPIO_Drive_pinNum, driveType_dt Solenoid_DriveType, gpio_num_t GPIO_Enable_pinNum, uint8_t coupled_HW_Driver_Idx)
                        :GPIO_Drive_pinNum(GPIO_Drive_pinNum), Solenoid_DriveType(Solenoid_DriveType), GPIO_Enable_pinNum(GPIO_Enable_pinNum), coupled_HW_Driver_Idx(coupled_HW_Driver_Idx)
{
    this->pin_OutputLevel = LOW;
    this->forced = false;
    this->forcedState = LOW;
}

bool HW_Driver::set_HwState(uint8_t state)
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

ScheduleAlarm::ScheduleAlarm(uint8_t h, uint8_t m, uint16_t period, uint8_t dow, uint8_t zones, HW_Driver* HW_Driver_Data)
                                :hours(h), minutes(m), period(period), dow(dow), zones(zones), HW_Driver_Data(HW_Driver_Data)
{

}

ScheduleAlarm::~ScheduleAlarm()
{
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
        nvsPrefs.putUChar((prefix + "h").c_str(), ScheduleAlarm_arr[i].getHours());
        nvsPrefs.putUChar((prefix + "m").c_str(), ScheduleAlarm_arr[i].getMinutes());
        nvsPrefs.putUShort((prefix + "p").c_str(), ScheduleAlarm_arr[i].getPeriod());
        nvsPrefs.putUChar((prefix + "d").c_str(), ScheduleAlarm_arr[i].getDow());
        nvsPrefs.putUChar((prefix + "z").c_str(), ScheduleAlarm_arr[i].getZones());

        /* save driver mapping */
        HW_Driver* hw = ScheduleAlarm_arr[i].getHwDriver();
        uint8_t driverIdx = 0;
        for (uint8_t d = 0; d < CFGM_MAX_DRIVERS; d++)
        {
            if (hw == &HW_Driver_arr[d]) { driverIdx = d; break; }
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
        uint8_t h = nvsPrefs.getUChar((prefix + "h").c_str(), ScheduleAlarm_arr[i].getHours());
        uint8_t m = nvsPrefs.getUChar((prefix + "m").c_str(), ScheduleAlarm_arr[i].getMinutes());
        uint16_t p = nvsPrefs.getUShort((prefix + "p").c_str(), ScheduleAlarm_arr[i].getPeriod());
        uint8_t d = nvsPrefs.getUChar((prefix + "d").c_str(), ScheduleAlarm_arr[i].getDow());
        uint8_t z = nvsPrefs.getUChar((prefix + "z").c_str(), ScheduleAlarm_arr[i].getZones());

        /* validate loaded values — NVS corruption can produce garbage */
        if (h < 24 && m < 60 && p >= 15)
        {
            ScheduleAlarm_arr[i].setHours(h);
            ScheduleAlarm_arr[i].setMinutes(m);
            ScheduleAlarm_arr[i].setPeriod(p);
            ScheduleAlarm_arr[i].setDow(d);
            ScheduleAlarm_arr[i].setZones(z);
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
            ScheduleAlarm_arr[i].setDow(0);
        }

        /* restore driver mapping */
        uint8_t drv = nvsPrefs.getUChar((prefix + "drv").c_str(), 0xFF);
        if (drv < CFGM_MAX_DRIVERS)
        {
            ScheduleAlarm_arr[i].setHwDriver(&HW_Driver_arr[drv]);
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

    ScheduleAlarm_arr[alarmId].setHours(h);
    ScheduleAlarm_arr[alarmId].setMinutes(m);
    ScheduleAlarm_arr[alarmId].setPeriod(period);
    ScheduleAlarm_arr[alarmId].setDow(dow);
    ScheduleAlarm_arr[alarmId].setZones(zones);

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

    ScheduleAlarm_arr[alarmId].setHwDriver(&HW_Driver_arr[driverId]);

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

void CfgM_MainFunction(void)
{
    if (nvsDirty)
    {
        nvsDirty = false;
        CfgM_SaveToNvs();
    }
}