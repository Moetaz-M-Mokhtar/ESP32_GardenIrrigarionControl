#ifndef _HWABSTR_HPP_
#define _HWABSTR_HPP_

#include <stdint.h>
#include <driver/gpio.h>
#include <ErrM.hpp>

/*********************define*********************/
#define HWABSTR_PAIRING_BUTTON_PIN   GPIO_NUM_32   /* active LOW: press to enter pairing mode */
#define HWABSTR_PAIRING_TIMEOUT_SEC  30            /* 30 seconds pairing window */
#define HWABSTR_RTC_INTERRUPT_PIN    GPIO_NUM_4    /* connected to DS3231 SQW pin */
#define HWABSTR_MAX_DRIVERS          4

/*********************typeDef*********************/
typedef enum
{
    GPIO_DRIVE = 0,
    LATCH_SN7475N_DRIVE,
    LATCH_SN7475N_DECODER
} driveType_dt;

typedef enum
{
    FORCE_NOT_FORCED = 0xFF,
    FORCE_LOW = 0,
    FORCE_HIGH = 1
} ForceState;

class HW_Driver
{
    public:
    gpio_num_t GPIO_Drive_pinNum;
    driveType_dt Solenoid_DriveType;
    gpio_num_t GPIO_Enable_pinNum;
    volatile uint8_t pin_OutputLevel;
    uint8_t coupled_HW_Driver_Idx;
    volatile bool forced;
    volatile uint8_t forcedState;
    HW_Driver(gpio_num_t GPIO_Drive_pinNum, \
                  driveType_dt Solenoid_DriveType, \
                  gpio_num_t GPIO_Enable_pinNum, \
                  uint8_t coupled_HW_Driver_Idx);
    bool set_HwState(uint8_t state);
};

extern HW_Driver HW_Driver_arr[HWABSTR_MAX_DRIVERS];

/*********************global function declaration*********************/
void HwAbstr_Init(void);
void HwAbstr_MainFunction(void);
bool HwAbstr_isRtcWake(void);
void HwAbstr_GoToDeepSleep(uint32_t sleepSeconds);
int HwAbstr_GetBootCount(void);
bool HwAbstr_isPairingButtonHeld(void);

/* force management — owned by HwAbstr, called from BLE task and main loop */
void HwAbstr_setForce(uint8_t driverId, uint8_t state, uint32_t duration);
void HwAbstr_clearForce(uint8_t driverId);
bool HwAbstr_isDriverForced(uint8_t driverId);
bool HwAbstr_hasActiveForces(void);

/* timer getters — read by BleComm for JSON status serialization */
uint32_t HwAbstr_getForceTimerStart(uint8_t driverId);
uint32_t HwAbstr_getForceTimerDuration(uint8_t driverId);

#endif /* _HWABSTR_HPP_ */