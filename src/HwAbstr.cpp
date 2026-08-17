/* include files */
#include <HwAbstr.hpp>
#include <CfgM.hpp>
#include <ErrM.hpp>
#include <BleComm.hpp>
#include <esp_sleep.h>
#include <driver/gpio.h>
/**************************************** define ***************************************/
#define HWABSTR_SERIAL_BAUDRATE       9600
/********************************* local type definition *******************************/

/****************************** local variable declaration *****************************/
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR bool wasRtcWake = false;

/******************************* local function declaration *****************************/

/****************************** local function definition *****************************/

static void SerialCommunicationInit()
{
    Serial.begin(HWABSTR_SERIAL_BAUDRATE);
    delay(100);
}
 

static void GPIO_PinModeInit()
{
    /* set all driver pins to OUTPUT mode without writing levels.
       On ESP32, pinMode(OUTPUT) preserves the current pin level.
       This is safe after deep sleep wake — the SN7475N shift register
       maintains its latched output independently. */
    uint8_t HWDrivers_count = sizeof(HW_Driver_arr) / sizeof(HW_Driver);

    for (uint8_t loopCounter = 0; loopCounter < HWDrivers_count; loopCounter++)
    {
        pinMode(HW_Driver_arr[loopCounter].GPIO_Drive_pinNum, OUTPUT);

        if (HW_Driver_arr[loopCounter].Solenoid_DriveType == LATCH_SN7475N_DRIVE)
        {
            pinMode(HW_Driver_arr[loopCounter].GPIO_Enable_pinNum, OUTPUT);
        }
    }
}

static void GPIO_FullInit()
{
    /* full initialization: set pin modes + drive all pins to known state (LOW).
       Used only on normal reset (not RTC wake) to establish a clean baseline. */
    pinMode(HWABSTR_RTC_INTERRUPT_PIN, INPUT_PULLUP);

    uint8_t HWDrivers_count = sizeof(HW_Driver_arr) / sizeof(HW_Driver);

    for (uint8_t loopCounter = 0; loopCounter < HWDrivers_count; loopCounter++)
    {
        pinMode(HW_Driver_arr[loopCounter].GPIO_Drive_pinNum, OUTPUT);
        digitalWrite(HW_Driver_arr[loopCounter].GPIO_Drive_pinNum, LOW);

        if (HW_Driver_arr[loopCounter].Solenoid_DriveType == LATCH_SN7475N_DRIVE)
        {
            pinMode(HW_Driver_arr[loopCounter].GPIO_Enable_pinNum, OUTPUT);
            digitalWrite(HW_Driver_arr[loopCounter].GPIO_Enable_pinNum, LOW);
            digitalWrite(HW_Driver_arr[loopCounter].GPIO_Enable_pinNum, HIGH);
            usleep(1000);
            digitalWrite(HW_Driver_arr[loopCounter].GPIO_Enable_pinNum,LOW);
            usleep(1000);
        }
    }
}

static void HWAbstr_holdDriverPinsForSleep(void)
{
    uint8_t HWDrivers_count = sizeof(HW_Driver_arr) / sizeof(HW_Driver);

    /* set all data lines LOW and enable pins LOW (latch mode).
       The SN7475N latch holds the last latched states, so zones keep
       their intended state through deep sleep. */
    for (uint8_t loopCounter = 0; loopCounter < HWDrivers_count; loopCounter++)
    {
        digitalWrite(HW_Driver_arr[loopCounter].GPIO_Drive_pinNum, LOW);

        if (HW_Driver_arr[loopCounter].Solenoid_DriveType == LATCH_SN7475N_DRIVE)
        {
            digitalWrite(HW_Driver_arr[loopCounter].GPIO_Enable_pinNum, LOW);
        }
    }

    /* hold the pins so they keep LOW during deep sleep. Without hold, ESP32
       GPIOs float — E12/E34 would read HIGH → transparent mode → all Q
       outputs follow floating D inputs (HIGH) → all zones ON. */
    for (uint8_t loopCounter = 0; loopCounter < HWDrivers_count; loopCounter++)
    {
        gpio_hold_en(HW_Driver_arr[loopCounter].GPIO_Drive_pinNum);

        if (HW_Driver_arr[loopCounter].Solenoid_DriveType == LATCH_SN7475N_DRIVE)
        {
            gpio_hold_en(HW_Driver_arr[loopCounter].GPIO_Enable_pinNum);
        }
    }

    /* enable deep sleep hold for all pins marked above */
    gpio_deep_sleep_hold_en();
}

static void HWAbstr_releaseDriverPins(void)
{
    uint8_t HWDrivers_count = sizeof(HW_Driver_arr) / sizeof(HW_Driver);

    /* disable deep sleep hold globally, then per-pin.
       Must be called after wake before any digitalWrite, otherwise the
       hold overrides the GPIO output state. */
    gpio_deep_sleep_hold_dis();

    for (uint8_t loopCounter = 0; loopCounter < HWDrivers_count; loopCounter++)
    {
        gpio_hold_dis(HW_Driver_arr[loopCounter].GPIO_Drive_pinNum);

        if (HW_Driver_arr[loopCounter].Solenoid_DriveType == LATCH_SN7475N_DRIVE)
        {
            gpio_hold_dis(HW_Driver_arr[loopCounter].GPIO_Enable_pinNum);
        }
    }
}

bool HwAbstr_isRtcWake(void)
{
    return wasRtcWake;
}

void HwAbstr_GoToDeepSleep(uint32_t sleepSeconds)
{
    Serial.printf("HwAbstr: Going to deep sleep for %lu seconds\n", sleepSeconds);

    /* Stop BLE advertising before sleep — deep sleep handles BT shutdown */
    BleComm_stopAdvertising();

    /* Set driver pins LOW and hold them through deep sleep.
       Without hold the GPIOs float → SN7475N enables read HIGH →
       transparent mode → all zones turn on. */
    HWAbstr_holdDriverPinsForSleep();

    /* Configure RTC alarm as wake source (EXT0 = GPIO level wake) */
    esp_sleep_enable_ext0_wakeup(HWABSTR_RTC_INTERRUPT_PIN, 0); /* wake on LOW (alarm active) */

    /* Also enable timer as backup in case RTC alarm was already cleared */
    esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000ULL);

    /* wasRtcWake is set in HwAbstr_Init() based on actual wake cause,
       not here — timer wake must set wasRtcWake=false */
    esp_deep_sleep_start();
}

static void HWAbstr_updateGPIOPinStates(void)
{
    uint8_t HWDrivers_count = sizeof(HW_Driver_arr) / sizeof(HW_Driver);
    bool GPIO_DRIVE_HW_active = false;

    /* Process coupled pairs together. Each pair shares an enable pin (SN7475N).
       Both data lines must be set before the enable pulse so the latch captures
       the correct state for both channels. */
    for (uint8_t i = 0; i < HWDrivers_count; i++)
    {
        if (HW_Driver_arr[i].Solenoid_DriveType == GPIO_DRIVE)
        {
            digitalWrite(HW_Driver_arr[i].GPIO_Drive_pinNum, HW_Driver_arr[i].pin_OutputLevel);
            GPIO_DRIVE_HW_active |= HW_Driver_arr[i].pin_OutputLevel == HIGH;
        }
        else if (HW_Driver_arr[i].Solenoid_DriveType == LATCH_SN7475N_DRIVE)
        {
            uint8_t coupledIdx = HW_Driver_arr[i].coupled_HW_Driver_Idx;

            /* process each coupled pair once (lower-numbered driver owns the enable) */
            if (coupledIdx > i && coupledIdx < HWDrivers_count)
            {
                /* set both data lines from intended states */
                digitalWrite(HW_Driver_arr[i].GPIO_Drive_pinNum, HW_Driver_arr[i].pin_OutputLevel);
                digitalWrite(HW_Driver_arr[coupledIdx].GPIO_Drive_pinNum, HW_Driver_arr[coupledIdx].pin_OutputLevel);

                /* pulse enable latch — both SN7475N channels capture */
                usleep(1000);
                digitalWrite(HW_Driver_arr[i].GPIO_Enable_pinNum, HIGH);
                usleep(1000);
                digitalWrite(HW_Driver_arr[i].GPIO_Enable_pinNum, LOW);
                usleep(1000);
            }
            /* skip higher-numbered driver in pair — already processed above */
        }
    }

    ErrM_SetErrorStatus(ERRM_DIRECT_GPIO_ALARM_ACTIVE, GPIO_DRIVE_HW_active);
}

static void HWAbstr_evaluateforcedStates(void)
{
    for (uint8_t i = 0; i < CFGM_MAX_DRIVERS; i++)
    {
        HW_Driver* driver = &HW_Driver_arr[i];

        if (driver->forced)
        {
            driver->set_HwState(driver->forcedState);
        }
    }
}
/****************************** global function declaration ****************************/
void HwAbstr_Init(void)
{
    SerialCommunicationInit();

    /* Release deep-sleep pin holds from previous sleep cycle (if any).
       Holds would override digitalWrite, so this must run before any
       GPIO output. Safe no-op on fresh power-on. */
    HWAbstr_releaseDriverPins();

    /* Check wake source */
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_EXT0)
    {
        wasRtcWake = true;
        Serial.printf("\n------------------ rtc wake %d ------------------\n", bootCount++);
        Serial.println("HwAbstr: Woke from RTC alarm");

        /* RTC wake: pin modes only — SN7475N latch holds state through sleep,
           no digitalWrite or enable pulse (would de-latch the shift register) */
        GPIO_PinModeInit();
    }
    else
    {
        wasRtcWake = false;
        Serial.printf("\n------------------ reset %d ------------------\n", bootCount++);
        Serial.println("HwAbstr: Normal power-on/reset");

        /* Pairing mode entry is handled by BleComm_Init() after clock sync */

        /* Check pairing button (active LOW) - if held, also clear paired state */
        pinMode(HWABSTR_PAIRING_BUTTON_PIN, INPUT_PULLUP);
        delay(50); /* debounce */
        if (digitalRead(HWABSTR_PAIRING_BUTTON_PIN) == LOW)
        {
            CfgM_ClearPaired();
            BleComm_enterPairingMode();
            Serial.println("HwAbstr: Pairing button held - cleared paired state");
        }

        /* Normal reset: full GPIO init — establish known state from scratch */
        GPIO_FullInit();
    }
}


void HwAbstr_MainFunction(void)
{
    HWAbstr_evaluateforcedStates();
    HWAbstr_updateGPIOPinStates();
}

int HwAbstr_GetBootCount(void)
{
    return bootCount;
}
