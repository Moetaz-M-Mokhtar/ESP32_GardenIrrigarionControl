/* include files */
#include <ErrM.hpp>
#include <stdint.h>
#include <TimerCtrl.hpp>
#include <RTClib.h>
/**************************************** define ***************************************/

/********************************* local type definition *******************************/
enum ErrM_ErrorLogType
{
    ERRM_NO_LOG,
    ERRM_INFO,
    ERRM_WARN,
    ERRM_CRITICAL
};

typedef struct
{
    const uint8_t errID;
    const ErrM_ErrorLogType errType; 
    const ErrM_Func_ID functionInhibition;
    const String errorText;
} ErrM_ErrorCfg;

/****************************** local variable declaration *****************************/
static ErrM_ErrorCfg ErrM_ErrorCfgList[ERRM_ERROR_COUNT] = 
{
    {
        //ERRM_NO_ERROR
        0x00,             //errID
        ERRM_NO_LOG,      //errType
        ERRM_FUNC_NONE,   //functionInhibition
        "NO_ERROR"        //errorText
    },
    {
        //ERRM_RTC_NOT_CONNECTED
        0x10,                 //errID
        ERRM_CRITICAL,        //errType
        ERRM_FUNC_SPRINKLE,   //functionInhibition
        "RTC not found"       //errorText
    },
    {
        //ERRM_RTC_LOST_POWER
        0x11,                //errID
        ERRM_WARN,           //errType
        ERRM_FUNC_NONE,      //functionInhibition
        "RTC lost power"
    },
};

static bool ErrM_ErrorStatus[ERRM_ERROR_COUNT] = 
{
    false,  //ERRM_NO_ERROR
    false,  //ERRM_RTC_NOT_CONNECTED
    false   //ERRM_RTC_LOST_POWER
};

static bool ErrM_FunctionPermission[ERRM_FUNC_COUNT] = 
{
    false,  //ERRM_FUNC_NONE
    true,   //ERRM_FUNC_SPRINKLE
    true    //ERRM_FUNC_SPCONN
};
/******************************* local function declaration *****************************/

/****************************** local function definition *****************************/

/****************************** global function declaration ****************************/
bool ErrM_GetFunctionPermission(ErrM_Func_ID functionID)
{
    return (functionID < ERRM_FUNC_COUNT) ?  ErrM_FunctionPermission[functionID] : false;
}

bool ErrM_SetErrorStatus(ErrM_Error_ID errorID, bool errorStatus)
{
    bool OpStatus = true;
    if(errorID < ERRM_ERROR_COUNT)
    {
        if(ErrM_ErrorStatus[errorID] != errorStatus)
        {
            /* error status modified */
            //log new error status
            if(ErrM_ErrorCfgList[errorID].errType != ERRM_NO_LOG)
            {
                //time stamp
                Serial.print(TimerCtrl_getCurrentTime().toString("DDD, DD MMM YYYY hh:mm:ss ap"));
                Serial.print(" ");

                //prefix
                if(ErrM_ErrorCfgList[errorID].errType == ERRM_INFO)
                    Serial.print("INFO: ");
                else if(ErrM_ErrorCfgList[errorID].errType == ERRM_WARN)
                    Serial.print("WARN: ");
                else
                    Serial.print("ERROR: ");

                //error message
                Serial.printf("0x%x %s %s\n", 
                                ErrM_ErrorCfgList[errorID].errID, 
                                ErrM_ErrorCfgList[errorID].errorText,
                                (errorStatus == true) ? "Active" : "Passive");
            }
            //update error status
            ErrM_ErrorStatus[errorID] = errorStatus;
        }
        else
        {
            /* nothing needs to be done if state was reported before */
        }
    }
    else
    {
        OpStatus = false;
    }
    return OpStatus;
}

void ErrM_Init(void)
{
    
}

void ErrM_mainFunction(void)
{
    uint8_t loopIndex = 0;
    bool ErrM_Function_Error_class_status[ERRM_FUNC_COUNT] = {false};

    for(loopIndex = 0; loopIndex < ERRM_ERROR_COUNT; loopIndex++)
    {
        if(ErrM_ErrorStatus[loopIndex] == true)
        {
            ErrM_Function_Error_class_status[ErrM_ErrorCfgList[loopIndex].functionInhibition] = true;
        }
    }

    for(loopIndex = 1; loopIndex < ERRM_FUNC_COUNT; loopIndex++)
    {
        //starting index is 1 to avoid modifications to ERRM_FUNC_NONE
        ErrM_FunctionPermission[loopIndex] = !(ErrM_Function_Error_class_status[loopIndex]);
    }
}