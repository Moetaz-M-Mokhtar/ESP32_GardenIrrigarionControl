#ifndef _ERRM_HPP_
#define _ERRM_HPP_
#include <stdint.h>

/*********************define*********************/

/*********************typeDef*********************/
enum ErrM_Error_ID
{
    ERRM_NO_ERROR = 0,
    ERRM_RTC_NOT_CONNECTED,         // 1 - CRITICAL - inhibits TIMERCTRL
    ERRM_RTC_LOST_POWER,            // 2 - WARN
    ERRM_DIRECT_GPIO_ALARM_ACTIVE,  // 3 - INFO - inhibits DEEP_SLEEP
    ERRM_NVS_LOAD_CORRUPTED,        // 4 - WARN - config slot corrupted
    ERRM_RTC_I2C_FAILED,            // 5 - CRITICAL - inhibits TIMERCTRL
    ERRM_BLE_ADVERTISING_FAILED,    // 6 - WARN
    ERRM_ERROR_COUNT
};

enum ErrM_Func_ID
{
    ERRM_FUNC_NONE = 0,
    ERRM_FUNC_SCHEDULER,
    ERRM_FUNC_TIMERCTRL,
    ERRM_FUNC_SPCONN,
    ERRM_FUNC_DEEP_SLEEP,
    ERRM_FUNC_COUNT
};

/*********************global function declaration*********************/
extern bool ErrM_GetFunctionPermission(ErrM_Func_ID functionID);
extern bool ErrM_SetErrorStatus(ErrM_Error_ID errorID, bool errorStatus);
extern bool ErrM_GetErrorStatus(ErrM_Error_ID errorID);
extern void ErrM_Init(void);
extern void ErrM_mainFunction(void);

#endif /* _ERRM_HPP_ */
