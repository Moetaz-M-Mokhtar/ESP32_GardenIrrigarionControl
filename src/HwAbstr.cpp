/* include files */
#include <Arduino.h>
#include <HwAbstr.hpp>
#include <ErrM.hpp>
#include <ClockDrift.hpp>
#include <Common.hpp>
#include <esp_sleep.h>
#include <driver/gpio.h>
/********************************* local type definition *******************************/

/****************************** local variable declaration *****************************/
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR bool wasRtcWake = false;

/* force timer tracking — millis-based for short durations (minutes/hours) */
static volatile uint32_t forceTimerStart[HWABSTR_MAX_DRIVERS] = {0};
static volatile uint32_t forceTimerDuration[HWABSTR_MAX_DRIVERS] = {0};

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

/******************************* local function declaration *****************************/

/****************************** local function definition *****************************/

static void SerialCommunicationInit()
{
    Serial.begin(COMMON_SERIAL_BAUDRATE);
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

void HwAbstr_GoToDeepSleep(void)
{
    HWAbstr_holdDriverPinsForSleep();

    /* RTC alarm is the primary wake source (set by Scheduler) */
    esp_sleep_enable_ext0_wakeup(HWABSTR_RTC_INTERRUPT_PIN, 0);

    /* Timer backup — wake after fallback period in case RTC alarm was missed */
    esp_sleep_enable_timer_wakeup(COMMON_SLEEP_FALLBACK_SEC * 1000000ULL);

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

static void HwAbstr_checkForceExpiry(void)
{
    uint32_t now = millis();

    for (uint8_t i = 0; i < HWABSTR_MAX_DRIVERS; i++)
    {
        if (!HW_Driver_arr[i].forced) continue;
        if (forceTimerDuration[i] == 0) continue;  /* indefinite force */

        uint32_t elapsed = now - forceTimerStart[i];
        if (elapsed >= forceTimerDuration[i])
        {
            Serial.printf("HwAbstr: Force expired for driver %d\n", i);
            HW_Driver_arr[i].forced = false;
            HW_Driver_arr[i].forcedState = 0;
            HW_Driver_arr[i].pin_OutputLevel = LOW;
            forceTimerStart[i] = 0;
            forceTimerDuration[i] = 0;
        }
    }
}

/****************************** global function definition ****************************/
void HwAbstr_setForce(uint8_t driverId, uint8_t state, uint32_t duration)
{
    if (driverId >= HWABSTR_MAX_DRIVERS) return;
    if (state != LOW && state != HIGH) return;

    HW_Driver_arr[driverId].forced = true;
    HW_Driver_arr[driverId].forcedState = state;
    forceTimerStart[driverId] = millis();
    forceTimerDuration[driverId] = duration;
}

void HwAbstr_clearForce(uint8_t driverId)
{
    if (driverId >= HWABSTR_MAX_DRIVERS) return;

    HW_Driver_arr[driverId].forced = false;
    HW_Driver_arr[driverId].forcedState = 0;
    forceTimerStart[driverId] = 0;
    forceTimerDuration[driverId] = 0;
}

bool HwAbstr_isDriverForced(uint8_t driverId)
{
    if (driverId >= HWABSTR_MAX_DRIVERS) return false;
    return HW_Driver_arr[driverId].forced;
}

uint8_t HwAbstr_getForceState(uint8_t driverId)
{
    if (driverId >= HWABSTR_MAX_DRIVERS) return 0;
    return HW_Driver_arr[driverId].forcedState;
}

bool HwAbstr_hasActiveForces(void)
{
    for (uint8_t i = 0; i < HWABSTR_MAX_DRIVERS; i++)
    {
        if (HW_Driver_arr[i].forced) return true;
    }
    return false;
}

uint32_t HwAbstr_getForceTimerStart(uint8_t driverId)
{
    if (driverId >= HWABSTR_MAX_DRIVERS) return 0;
    return forceTimerStart[driverId];
}

uint32_t HwAbstr_getForceTimerDuration(uint8_t driverId)
{
    if (driverId >= HWABSTR_MAX_DRIVERS) return 0;
    return forceTimerDuration[driverId];
}

void HwAbstr_setRequestedState(uint8_t driverId, uint8_t state)
{
    if (driverId >= HWABSTR_MAX_DRIVERS) return;
    if (HW_Driver_arr[driverId].forced) return;

    HW_Driver_arr[driverId].pin_OutputLevel = state;
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

        GPIO_PinModeInit();
    }
    else
    {
        wasRtcWake = false;
        Serial.printf("\n------------------ reset %d ------------------\n", bootCount++);
        Serial.println("HwAbstr: Normal power-on/reset");

        /* Normal reset: full GPIO init — establish known state from scratch */
        GPIO_FullInit();
    }
}


void HwAbstr_MainFunction(void)
{
    HwAbstr_checkForceExpiry();

    for (uint8_t i = 0; i < HWABSTR_MAX_DRIVERS; i++)
    {
        if (HW_Driver_arr[i].forced)
        {
            HW_Driver_arr[i].set_HwState(HW_Driver_arr[i].forcedState);
        }
    }

    HWAbstr_updateGPIOPinStates();
}

int HwAbstr_GetBootCount(void)
{
    return bootCount;
}
