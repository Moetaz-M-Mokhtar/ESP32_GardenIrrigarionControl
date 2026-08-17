/* include files */
#include <CfgM.hpp>
#include <ErrM.hpp>
#include <Common.hpp>
#include <Preferences.h>
/**************************************** define ***************************************/
#define NVS_NAMESPACE "cfgm"
#define NVS_KEY_ALARM_INITIALIZED "alarm_init"

/****************************** global function definition ****************************/
void CfgM_Init(void)
{
    static Preferences nvsPrefs;
    nvsPrefs.begin(NVS_NAMESPACE, false);
    CfgM_LoadFromNvs();
}

void CfgM_SaveToNvs(void)
{
    static Preferences nvsPrefs;
    nvsPrefs.putBool(NVS_KEY_ALARM_INITIALIZED, true);

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
}

void CfgM_LoadFromNvs(void)
{
    static Preferences nvsPrefs;

    /* Load alarm configuration */
    if (!nvsPrefs.getBool(NVS_KEY_ALARM_INITIALIZED, false))
    {
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
        if (h < 24 && m < 60 && p >= COMMON_MIN_ALARM_PERIOD_MIN)
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
            Serial.printf("CfgM: Sched %d corrupted (h=%d m=%d p=%d d=0x%02X), reset\n", i, h, m, p, d);
            ErrM_SetErrorStatus(ERRM_NVS_LOAD_CORRUPTED, true);
            ScheduleAlarm_arr[i].setDow(0);
        }

        /* restore driver mapping */
        uint8_t drv = nvsPrefs.getUChar((prefix + "drv").c_str(), 0xFF);
        if (drv < HWABSTR_MAX_DRIVERS)
        {
            ScheduleAlarm_arr[i].setHwDriver(&HW_Driver_arr[drv]);
        }
    }
}

bool CfgM_SetAlarm(uint8_t alarmId, uint8_t h, uint8_t m, uint16_t period, uint8_t dow, uint8_t zones)
{
    if (alarmId >= SCHEDULER_MAX_ALARMS)
    {
        return false;
    }

    if (h >= 24 || m >= 60 || period < COMMON_MIN_ALARM_PERIOD_MIN)
    {
        return false;
    }

    ScheduleAlarm_arr[alarmId].setHours(h);
    ScheduleAlarm_arr[alarmId].setMinutes(m);
    ScheduleAlarm_arr[alarmId].setPeriod(period);
    ScheduleAlarm_arr[alarmId].setDow(dow);
    ScheduleAlarm_arr[alarmId].setZones(zones);

    CfgM_SaveToNvs();
    return true;
}

bool CfgM_SetAlarmDriver(uint8_t alarmId, uint8_t driverId)
{
    if (alarmId >= SCHEDULER_MAX_ALARMS || driverId >= HWABSTR_MAX_DRIVERS)
    {
        return false;
    }

    ScheduleAlarm_arr[alarmId].setHwDriver(&HW_Driver_arr[driverId]);
    CfgM_SaveToNvs();
    return true;
}
