/* include files */
#include<HwAbstr.hpp>
#include<CfgM.hpp>
#include<ErrM.hpp>
#include <driver/rtc_io.h>
/**************************************** define ***************************************/
#define HWABSTR_SERIAL_BAUDRATE       9600

#define HWABSTR_RTC_INTERRUPT_PIN     GPIO_NUM_4               /* connected to RTC SQW pin */
/********************************* local type definition *******************************/

/****************************** local variable declaration *****************************/
RTC_DATA_ATTR int bootCount = 0;
/******************************* local function declaration *****************************/

/****************************** local function definition *****************************/

static void SerialCommunicationInit()
{
    Serial.begin(HWABSTR_SERIAL_BAUDRATE);
    delay(100);
}
 

static void GPIO_Initialization()
{    
    // setting PIN mode for RTC alarm
    pinMode(HWABSTR_RTC_INTERRUPT_PIN, INPUT_PULLUP);
    
    //setting deep sleep wakeup source
    esp_sleep_enable_ext0_wakeup(HWABSTR_RTC_INTERRUPT_PIN, LOW);

    // setting Driver control pins as output
    uint8_t HWDrivers_count = sizeof(HW_Driver_cfg_arr) / sizeof(HW_Driver_cfg);

    for (uint8_t loopCounter = 0; loopCounter < HWDrivers_count; loopCounter++)
    {
        pinMode(HW_Driver_cfg_arr[loopCounter].GPIO_Drive_pinNum, OUTPUT);
        digitalWrite(HW_Driver_cfg_arr[loopCounter].GPIO_Drive_pinNum, LOW);
        
        if (HW_Driver_cfg_arr[loopCounter].Solenoid_DriveType == LATCH_SN7475N_DRIVE)
        {
            pinMode(HW_Driver_cfg_arr[loopCounter].GPIO_Enable_pinNum, OUTPUT);
            digitalWrite(HW_Driver_cfg_arr[loopCounter].GPIO_Enable_pinNum, LOW);
        }
    }
}

static void HwAbstr_GoToSleep(void)
{
    Serial.print("TimerCtrlState: ");
    Serial.print(ErrM_GetFunctionPermission(ERRM_FUNC_TIMERCTRL));
    Serial.print("\n");
    Serial.print("Schedular State: ");
    Serial.print(ErrM_GetFunctionPermission(ERRM_FUNC_SCHEDULER));
    Serial.print("\n");
    Serial.println("----------------------------------------------");
    if (ErrM_GetFunctionPermission(ERRM_FUNC_DEEP_SLEEP) == true)
    {
        esp_deep_sleep_start();     //deep sleep
    }
    else
    {
        sleep(60);                  //sleep for 1 minute
    }
}

static void HWAbstr_updateGPIOPinStates(void)
{
    uint8_t HWDrivers_count = sizeof(HW_Driver_cfg_arr) / sizeof(HW_Driver_cfg);
    bool GPIO_DRIVE_HW_active = false;
    for(uint8_t loopCounter = 0; loopCounter < HWDrivers_count; loopCounter++)
    {
        if(HW_Driver_cfg_arr[loopCounter].Solenoid_DriveType == GPIO_DRIVE)
        {
            digitalWrite(HW_Driver_cfg_arr[loopCounter].GPIO_Drive_pinNum, HW_Driver_cfg_arr[loopCounter].pin_OutputLevel);
            if(HW_Driver_cfg_arr[loopCounter].pin_OutputLevel == HIGH)
            {
                GPIO_DRIVE_HW_active = true;
            }
        }
        if(HW_Driver_cfg_arr[loopCounter].Solenoid_DriveType == LATCH_SN7475N_DRIVE)
        {
            digitalWrite(HW_Driver_cfg_arr[loopCounter].GPIO_Drive_pinNum, HW_Driver_cfg_arr[loopCounter].pin_OutputLevel);
            if(HW_Driver_cfg_arr[loopCounter].coupled_HW_Driver_Idx < HWDrivers_count)
            {
                HW_Driver_cfg* coupled_HW_Driver = &(HW_Driver_cfg_arr[HW_Driver_cfg_arr[loopCounter].coupled_HW_Driver_Idx]);
                if(coupled_HW_Driver->coupled_HW_Driver_Idx == loopCounter)
                {
                    digitalWrite(coupled_HW_Driver->GPIO_Drive_pinNum, coupled_HW_Driver->pin_OutputLevel);
                }
            }
            digitalWrite(HW_Driver_cfg_arr[loopCounter].GPIO_Enable_pinNum, HIGH);
            usleep(100);
            digitalWrite(HW_Driver_cfg_arr[loopCounter].GPIO_Enable_pinNum,LOW);
        }
    }

    for(uint8_t loopCounter = 0; loopCounter < HWDrivers_count; loopCounter++)
    {
        // reset HW states after each cycle
        HW_Driver_cfg_arr[loopCounter].set_HwState(LOW);
    }

    //set Error if GPIO_DRIVE_HW_active is true and clear it if false
    ErrM_SetErrorStatus(ERRM_DIRECT_GPIO_ALARM_ACTIVE, GPIO_DRIVE_HW_active);
}
/****************************** global function declaration ****************************/
void HwAbstr_Init(void)
{
    SerialCommunicationInit();
    Serial.printf("\n------------------ reset %d ------------------\n", bootCount++);
    GPIO_Initialization();
}


void HwAbstr_MainFunction(void)
{
    HWAbstr_updateGPIOPinStates();
    HwAbstr_GoToSleep();
}