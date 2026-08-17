#include <DebugM.hpp>
#include <Arduino.h>
#include <CfgM.hpp>
#include <Scheduler.hpp>
#include <HwAbstr.hpp>
#include <TimerCtrl.hpp>
#include <ClockDrift.hpp>
#include <BleComm.hpp>
#include <ErrM.hpp>
#include <Common.hpp>

static uint32_t lastDumpTime = 0;
static bool dumpRequested = false;

/* DOW bitmask → SMTWTFS string (bit7=Sun..bit1=Sat, bit0=unused) */
static void DebugM_dowToString(uint8_t dow, char* buf)
{
    const char* labels = "SMTWTFS";
    for (uint8_t i = 0; i < 7; i++)
    {
        /* bit7=Sun(labels[0]), bit6=Mon(labels[1]), ..., bit1=Sat(labels[6]) */
        buf[i] = (dow & (1 << (7 - i))) ? labels[i] : '-';
    }
    buf[7] = '\0';
}

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
    bool shouldDump = dumpRequested || ((now - lastDumpTime) >= COMMON_DEBUGM_DUMP_INTERVAL_SEC);
    if (!shouldDump) return;

    dumpRequested = false;
    lastDumpTime = now;

    DateTime corrected = ClockDrift_getCorrectedTime();
    DateTime raw = TimerCtrl_getCurrentTime();

    Serial.println("\n=== SYSTEM STATUS ===");
    Serial.printf("  Time: %04d/%02d/%02d %02d:%02d:%02d | Raw: %lu\n",
                  corrected.year(), corrected.month(), corrected.day(),
                  corrected.hour(), corrected.minute(), corrected.second(),
                  raw.unixtime());
    Serial.printf("  Temp: %.1fC | Drift: %.2fppm | Sync: %lu\n",
                  TimerCtrl_getTemperature(), ClockDrift_getCoeff(),
                  ClockDrift_getLastSyncTime());
    Serial.printf("  Boot: %d | BLE: %s\n",
                  HwAbstr_GetBootCount(),
                  BleComm_isConnected() ? "conn" : "disc");

    /* RTC hardware alarms */
    for (uint8_t idx = TIMER1_INDEX; idx <= TIMER2_INDEX; idx++)
    {
        DateTime alarmTime = TimerCtrl_getAlarmTime(idx);
        Serial.printf("  RTC Alarm%d: %02d:%02d:%02d\n",
                      idx,
                      alarmTime.hour(), alarmTime.minute(), alarmTime.second());
    }

    /* Drivers */
    for (uint8_t i = 0; i < HWABSTR_MAX_DRIVERS; i++)
    {
        HW_Driver* drv = &HW_Driver_arr[i];
        const char* forcedStr = "-";
        if (drv->forced)
        {
            forcedStr = (drv->forcedState == 1) ? "H" : "L";
        }
        Serial.printf("  Drv%d: GPIO%d/%c%d out=%d sched=%s force=%s\n",
            i, drv->GPIO_Drive_pinNum,
            drv->Solenoid_DriveType == LATCH_SN7475N_DRIVE ? 'E' : 'D',
            drv->GPIO_Enable_pinNum,
            drv->pin_OutputLevel,
            Scheduler_IsDriverScheduledOn(i) ? "ON" : "-",
            forcedStr);
    }

    /* Schedules — only active ones */
    char dowBuf[8];
    for (uint8_t i = 0; i < SCHEDULER_MAX_ALARMS; i++)
    {
        ScheduleAlarm* a = &ScheduleAlarm_arr[i];
        if (a->getDow() == 0) continue;

        DebugM_dowToString(a->getDow(), dowBuf);
        bool today = Scheduler_isDowActive(a->getDow(), corrected.dayOfTheWeek());
        Serial.printf("  Sched%d: %02d:%02d p=%dm %s %s zones=0x%02X\n",
                       i, a->getHours(), a->getMinutes(), a->getPeriod(),
                       dowBuf, today ? "*" : " ",
                       a->getZones());
    }

    /* Errors */
    bool hasErrors = false;
    for (uint8_t i = 1; i < ERRM_ERROR_COUNT; i++)
    {
        if (ErrM_GetErrorStatus((ErrM_Error_ID)i))
        {
            if (!hasErrors) Serial.printf("  Errors:");
            Serial.printf(" [%d]", i);
            hasErrors = true;
        }
    }
    if (hasErrors) Serial.println();
    Serial.println("=== END ===\n");
}
