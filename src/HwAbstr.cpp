/* include files */
#include<HwAbstr.hpp>

/**************************************** define ***************************************/
#define HWABSTR_SERIAL_BAUDRATE       115200

#define HWABSTR_RTC_INTERRUPT_PIN     2                         /* connected to RTC SQW pin */
#define HWABSTR_SD_CS_PIN             5                         /* connected to SD card CS pin */
#define HWABSTR_SD_CFG_FILE           "/SprinklerCfg.json"      /* configuration file */
/********************************* local type definition *******************************/

/****************************** local variable declaration *****************************/
static JsonDocument cfgDoc;
static bool cfgDocValid = false;
/******************************* local function declaration *****************************/

/****************************** local function definition *****************************/
static void RTC_ISR()
{

}

static void SerialCommunicationInit()
{
    Serial.begin(HWABSTR_SERIAL_BAUDRATE);
}

static bool SDCard_Initialization()
{
    bool returnValue = true;

    // if(!SD.begin(HWABSTR_SD_CS_PIN))
    // {
    //     ErrM_SetErrorStatus(ERRM_SDCard_INIT, true);
    //     returnValue = false;
    // }
    // else
    // {
    //     File cfgFile = SD.open(HWABSTR_SD_CFG_FILE);
        
    //     DeserializationError error = deserializeJson(cfgDoc, cfgFile);
        
    //     if (error)
    //     {
    //         ErrM_SetErrorStatus(ERRM_SDCard_LOADING_FILE_FAILED, true);
    //         returnValue = false;
    //     }
    //     else
    //     {
    //         cfgDocValid = true;
    //     }
         
    //     cfgFile.close();
    // }

    // if(returnValue == true)
    // {
    //     ErrM_SetErrorStatus(ERRM_SDCard_INIT, false);
    //     ErrM_SetErrorStatus(ERRM_SDCard_LOADING_FILE_FAILED, false);
    // }

    return returnValue;
}

static bool EEPROM_Initialization()
{

} 

static void GPIO_Initialization()
{
    // Making it so, that the alarm will trigger an interrupt
    pinMode(HWABSTR_RTC_INTERRUPT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(HWABSTR_RTC_INTERRUPT_PIN), RTC_ISR, FALLING);
}
/****************************** global function declaration ****************************/
void HwAbstr_Init(void)
{
    bool returnValueSDcard = true;
    bool returnValueEEPROM = true;
    GPIO_Initialization();
    SerialCommunicationInit();
    returnValueSDcard = SDCard_Initialization();
    
    if(!returnValueSDcard)
    {
        returnValueEEPROM = EEPROM_Initialization();
    }

    if((!returnValueSDcard) && (!returnValueEEPROM))
    {
        ErrM_SetErrorStatus(ERRM_FAILED_TO_LOAD_CFG, true);
    }
}

bool HwAbstr_GetCfgString(JsonDocument* cfg)
{
    if(cfgDocValid == true)
    {
        *cfg = cfgDoc;
    }
    return cfgDocValid;
}