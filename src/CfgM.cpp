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

/****************************** local variable definition *****************************/
static volatile bool driverEnabled[HWABSTR_MAX_DRIVERS] = {true, true, true, true};
static Preferences nvsPrefs;

/****************************** global function definition ****************************/
void CfgM_Init(void)
{
    nvsPrefs.begin(NVS_NAMESPACE, false);
    CfgM_LoadFromNvs();
}

void CfgM_SaveToNvs(void)
{
    nvsPrefs.putBool(NVS_KEY_ALARM_INITIALIZED, true);
    nvsPrefs.putBool(NVS_KEY_DRIVER_INITIALIZED, true);

    for (uint8_t i = 0; i < SCHEDULER_MAX_ALARMS; i++)
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
        for (uint8_t d = 0; d < HWABSTR_MAX_DRIVERS; d++)
        {
            if (hw == &HW_Driver_arr[d]) { driverIdx = d; break; }
        }
        nvsPrefs.putUChar((prefix + "drv").c_str(), driverIdx);
    }

    for (uint8_t i = 0; i < HWABSTR_MAX_DRIVERS; i++)
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
        for (uint8_t i = 0; i < HWABSTR_MAX_DRIVERS; i++)
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

    for (uint8_t i = 0; i < SCHEDULER_MAX_ALARMS; i++)
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
        if (drv < HWABSTR_MAX_DRIVERS)
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
    if (alarmId >= SCHEDULER_MAX_ALARMS)
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

    CfgM_SaveToNvs();

    Serial.printf("CfgM: Alarm %d set to %02d:%02d, period=%d, dow=0x%02X, zones=0x%02X\n",
                   alarmId, h, m, period, dow, zones);

    return true;
}

bool CfgM_SetAlarmDriver(uint8_t alarmId, uint8_t driverId)
{
    if (alarmId >= SCHEDULER_MAX_ALARMS || driverId >= HWABSTR_MAX_DRIVERS)
    {
        Serial.printf("CfgM: Invalid alarm %d or driver %d\n", alarmId, driverId);
        return false;
    }

    ScheduleAlarm_arr[alarmId].setHwDriver(&HW_Driver_arr[driverId]);

    CfgM_SaveToNvs();

    Serial.printf("CfgM: Alarm %d assigned to driver %d\n", alarmId, driverId);
    return true;
}

bool CfgM_IsDriverEnabled(uint8_t driverId)
{
    if (driverId >= HWABSTR_MAX_DRIVERS)
    {
        return false;
    }
    return driverEnabled[driverId];
}

bool CfgM_SetDriverEnabled(uint8_t driverId, bool enabled)
{
    if (driverId >= HWABSTR_MAX_DRIVERS)
    {
        Serial.printf("CfgM: Invalid driver ID %d\n", driverId);
        return false;
    }

    driverEnabled[driverId] = enabled;
    
    CfgM_SaveToNvs();
    
    Serial.printf("CfgM: Driver %d %s\n", driverId, enabled ? "enabled" : "disabled");
    return true;
}