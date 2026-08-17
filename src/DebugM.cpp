#include <DebugM.hpp>
#include <Arduino.h>
#include <CfgM.hpp>
#include <Scheduler.hpp>
#include <HwAbstr.hpp>
#include <TimerCtrl.hpp>
#include <ClockDrift.hpp>
#include <BleComm.hpp>
#include <ErrM.hpp>

#define DEBUGM_CYCLE_SEC 10

static uint32_t lastDumpTime = 0;
static bool dumpRequested = false;

void DebugM_Init(void)
{
    lastDumpTime = 0;
    dumpRequested = false;
}

void DebugM_requestDump(void)
{
    dumpRequested = true;
}

void DebugM_mainFunction(void)
{
    uint32_t now = ClockDrift_getCorrectedTime().unixtime();
    bool shouldDump = dumpRequested || ((now - lastDumpTime) >= DEBUGM_CYCLE_SEC);
    if (!shouldDump) return;

    dumpRequested = false;
    lastDumpTime = now;

    DateTime corrected = ClockDrift_getCorrectedTime();
    DateTime raw = TimerCtrl_getCurrentTime();

    Serial.println("\n=== SYSTEM STATUS ===");
    Serial.printf("  Time: %04d/%02d/%02d (%d) %02d:%02d:%02d | Raw: %lu\n",
                  corrected.year(), corrected.month(), corrected.day(),
                  corrected.dayOfTheWeek(),
                  corrected.hour(), corrected.minute(), corrected.second(),
                  raw.unixtime());
    Serial.printf("  Temp: %.1fC | Drift: %.2f ppm | LastSync: %lu\n",
                  TimerCtrl_getTemperature(), ClockDrift_getCoeff(),
                  ClockDrift_getLastSyncTime());
    Serial.printf("  Boot: %d | BLE: %s | Pairing: %s\n",
                  HwAbstr_GetBootCount(),
                  BleComm_isConnected() ? "connected" : "disconnected",
                  BleComm_isPairingMode() ? "active" : "idle");

    /* Drivers */
    for (uint8_t i = 0; i < HWABSTR_MAX_DRIVERS; i++)
    {
        HW_Driver* drv = &HW_Driver_arr[i];
        const char* forcedStr = "NO";
        if (drv->forced)
        {
            forcedStr = (drv->forcedState == 1) ? "HIGH" : "LOW";
        }
        Serial.printf("  Drv[%d]: GPIO%d/%c%d | Out=%d Forced=%s | Scheduled=%s Enabled=%s\n",
            i, drv->GPIO_Drive_pinNum,
            drv->Solenoid_DriveType == LATCH_SN7475N_DRIVE ? 'E' : 'D',
            drv->GPIO_Enable_pinNum,
            drv->pin_OutputLevel,
            forcedStr,
            Scheduler_IsDriverScheduledOn(i) ? "YES" : "NO",
            CfgM_IsDriverEnabled(i) ? "YES" : "NO");
    }

    /* Alarms — only active ones */
    for (uint8_t i = 0; i < SCHEDULER_MAX_ALARMS; i++)
    {
        ScheduleAlarm* a = &ScheduleAlarm_arr[i];
        if (a->getDow() == 0) continue;

        bool dowActive = Scheduler_isDowActive(a->getDow(), corrected.dayOfTheWeek());
        Serial.printf("  Alarm[%d]: %02d:%02d period=%d dow=0x%02X zones=0x%02X dowNow=%s\n",
                       i, a->getHours(), a->getMinutes(), a->getPeriod(),
                       a->getDow(), a->getZones(),
                       dowActive ? "YES" : "no");
    }

    /* Errors */
    Serial.printf("  Errors:");
    for (uint8_t i = 1; i < ERRM_ERROR_COUNT; i++)
    {
        if (ErrM_GetErrorStatus((ErrM_Error_ID)i))
        {
            Serial.printf(" [%d]", i);
        }
    }
    Serial.println();
    Serial.println("=== END ===\n");
}
