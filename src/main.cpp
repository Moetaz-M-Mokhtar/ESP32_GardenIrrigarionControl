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
    if (HwAbstr_hasActiveForces()) return false;
    if (ErrM_GetErrorStatus(ERRM_DIRECT_GPIO_ALARM_ACTIVE)) return false;
    if (!ErrM_GetFunctionPermission(ERRM_FUNC_DEEP_SLEEP)) return false;
    return true;
}

static void goToSleep(void)
{
    Serial.println("Main: going to sleep");
    BleComm_stopAdvertising();
    HwAbstr_GoToDeepSleep();
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
