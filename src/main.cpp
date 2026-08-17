#include <Arduino.h>
#include <ErrM.hpp>
#include <CfgM.hpp>
#include <TimerCtrl.hpp>
#include <ClockDrift.hpp>
#include <BleComm.hpp>
#include <Scheduler.hpp>
#include <HwAbstr.hpp>
#include <DebugM.hpp>

static bool shouldSleep(void)
{
    if (HwAbstr_isRtcWake()) return true;
    if (BleComm_isConnected()) return false;
    if (!BleComm_isPairingTimeout()) return false;
    return true;
}

static void goToSleep(void)
{
    if (!ErrM_GetFunctionPermission(ERRM_FUNC_DEEP_SLEEP)) return;
    if (HwAbstr_hasActiveForces()) return;

    Scheduler_MainFunction();
    uint32_t sleepSec = Scheduler_GetSecondsUntilNextAlarm();
    if (sleepSec == 0) sleepSec = SCHEDULER_FALLBACK_SLEEP_SEC;
    Serial.printf("Main: goToSleep, next alarm in %lu seconds\n", sleepSec);

    BleComm_stopAdvertising();
    HwAbstr_GoToDeepSleep(sleepSec);
}

void setup()
{
    HwAbstr_Init();
    ErrM_Init();
    CfgM_Init();
    TimerCtrl_Init();
    ClockDrift_Init();
    DebugM_Init();

    /* Only init BLE during pairing window (non-RTC wake).
       On RTC wake we just run scheduler and go back to sleep. */
    if (!HwAbstr_isRtcWake())
    {
        BleComm_Init();

        /* Check if pairing button was held during boot — enter pairing mode */
        if (HwAbstr_isPairingButtonHeld())
        {
            BleComm_enterPairingMode();
            Serial.println("Main: Pairing button held - entered pairing mode");
        }
    }
}

void loop()
{
    ErrM_mainFunction();
    TimerCtrl_mainFunction();
    Scheduler_MainFunction();
    HwAbstr_MainFunction();
    BleComm_mainFunction();
    DebugM_mainFunction();

    if (shouldSleep())
    {
        goToSleep();
    }
    else
    {
        delay(BleComm_isConnected() ? 100 : 1000);
    }
}
