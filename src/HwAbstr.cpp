/* include files */
#include<HwAbstr.hpp>

/**************************************** define ***************************************/
#define HWABSTR_SERIAL_BAUDRATE       9600

#define HWABSTR_RTC_INTERRUPT_PIN     GPIO_NUM_33               /* connected to RTC SQW pin */
/********************************* local type definition *******************************/

/****************************** local variable declaration *****************************/

/******************************* local function declaration *****************************/

/****************************** local function definition *****************************/

static void SerialCommunicationInit()
{
    Serial.begin(HWABSTR_SERIAL_BAUDRATE);
}
 

static void GPIO_Initialization()
{
    // setting PIN mode for RTC alarm
    pinMode(HWABSTR_RTC_INTERRUPT_PIN, INPUT_PULLUP);

    //setting deep sleep wakeup source
    esp_sleep_enable_ext0_wakeup(HWABSTR_RTC_INTERRUPT_PIN, LOW);
}
/****************************** global function declaration ****************************/
void HwAbstr_Init(void)
{
    GPIO_Initialization();
    SerialCommunicationInit();
}

void HwAbstr_GoToSleep(void)
{
    gpio_deep_sleep_hold_en();  //enable pin hold during deep sleep
    esp_deep_sleep_start();     //deep sleep
}

void HWAbstr_updateGPIOPinState(gpio_num_t pinNumber, uint8_t state)
{
    gpio_hold_dis(pinNumber);
    pinMode(pinNumber, OUTPUT);     // set the pin as output
    digitalWrite(pinNumber, state); // write pin to desired output
    gpio_hold_en(pinNumber);
}