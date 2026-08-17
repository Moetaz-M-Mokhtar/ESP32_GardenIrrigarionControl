#include <Arduino.h>
#include <ErrM.hpp>
#include <CfgM.hpp>
#include <TimerCtrl.hpp>
#include <ClockDrift.hpp>
#include <BleComm.hpp>
#include <Scheduler.hpp>
#include <HwAbstr.hpp>

static void goToSleep(void)
{
    /* check error manager — ERRM_DIRECT_GPIO_ALARM_ACTIVE inhibits deep sleep
       to prevent shutting off GPIO-driven solenoids mid-operation */
    if (!ErrM_GetFunctionPermission(ERRM_FUNC_DEEP_SLEEP))
    {
        Serial.println("Main: ERRM inhibits deep sleep");
        return;
    }

    /* never sleep while forced actions are active */
    if (BleComm_hasActiveForces())
    {
        Serial.println("Main: Active forces prevent deep sleep");
        return;
    }

    Scheduler_MainFunction();

    uint32_t sleepSec = Scheduler_GetSecondsUntilNextAlarm();
    if (sleepSec == 0) sleepSec = SCHEDULER_FALLBACK_SLEEP_SEC;
    Serial.printf("Main: goToSleep called, next alarm in %lu seconds\n", sleepSec);
    HwAbstr_GoToDeepSleep(sleepSec);
}

void setup()
{
    HwAbstr_Init();
    ErrM_Init();
    CfgM_Init();
    TimerCtrl_Init();
    ClockDrift_Init();

    /* Only init BLE during pairing window (non-RTC wake).
       On RTC wake we just run scheduler and go back to sleep. */
    if (!HwAbstr_isRtcWake())
    {
        BleComm_Init();
    }
}

void loop()
{
    Serial.println("Main: loop start");
    if (HwAbstr_isRtcWake())
    {
        Serial.println("Main: RTC wake path");
        /* === RTC WAKE: automatic cycle === */
        ErrM_mainFunction();
        TimerCtrl_mainFunction();
        Scheduler_MainFunction();
        HwAbstr_MainFunction();
        BleComm_checkTimerExpiry();
        goToSleep();
    }
    else
    {
        Serial.println("Main: normal reset path");
        /* === NORMAL RESET: pairing window === */
        ErrM_mainFunction();
        TimerCtrl_mainFunction();

        if (BleComm_isConnected())
        {
            Serial.println("Main: BLE connected path");
            /* BLE connected — handle communication, stay awake.
               Run scheduler so scheduled zones get power even while connected. */
            BleComm_mainFunction();
            BleComm_checkTimerExpiry();
            Scheduler_MainFunction();
            HwAbstr_MainFunction();
            delay(100);
        }
        else if (BleComm_isPairingTimeout())
        {
            Serial.println("Main: pairing timeout path -> goToSleep");
            /* Pairing window expired — check timers, apply GPIO, then sleep or stay awake */
            BleComm_checkTimerExpiry();
            Scheduler_MainFunction();
            HwAbstr_MainFunction();
            goToSleep();
            delay(1000);
        }
        else
        {
            Serial.println("Main: pairing window active, advertising");
            /* Still in pairing window — ensure advertising is running.
               Apply scheduler state to hardware too, so an alarm that fires
               during the window turns its zone on immediately. */
            BleComm_startAdvertising();
            Scheduler_MainFunction();
            HwAbstr_MainFunction();
            BleComm_checkTimerExpiry();
            delay(1000);
        }
    }
}
