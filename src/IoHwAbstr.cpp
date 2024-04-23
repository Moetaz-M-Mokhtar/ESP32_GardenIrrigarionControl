/* include files */
#include<IoHwAbstr.hpp>

/**************************************** define ***************************************/
#define IOHWABSTR_SERIAL_BAUDRATE       115200

#define IOHWABSTR_RTC_INTERRUPT_PIN     2           /* connected to RTC SQW pin */
/********************************* local type definition *******************************/

/****************************** local variable declaration *****************************/

/******************************* local function declaration *****************************/

/****************************** local function definition *****************************/
static void RTC_ISR()
{

}

static void SerialCommunicationInit()
{
    Serial.begin(IOHWABSTR_SERIAL_BAUDRATE);
}

static void GPIO_Initialization()
{
    // Making it so, that the alarm will trigger an interrupt
    pinMode(IOHWABSTR_RTC_INTERRUPT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(IOHWABSTR_RTC_INTERRUPT_PIN), RTC_ISR, FALLING);
}
/****************************** global function declaration ****************************/
bool IoHwAbstr_Init(void)
{
    GPIO_Initialization();
    SerialCommunicationInit();
    return true;
}