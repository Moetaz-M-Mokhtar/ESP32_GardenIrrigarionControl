#ifndef _ERRM_HPP_
#define _ERRM_HPP_
#include <stdint.h>

/*********************define*********************/

/*********************typeDef*********************/
enum ErrM_Error_ID
{
    ERRM_NO_ERROR = 0,
    ERRM_RTC_NOT_CONNECTED,
    ERRM_RTC_LOST_POWER,
    ERRM_ERROR_COUNT
};

enum ErrM_Func_ID
{
    ERRM_FUNC_NONE = 0,
    ERRM_FUNC_SPRINKLE,
    ERRM_FUNC_SPCONN,
    ERRM_FUNC_COUNT
};

/*********************global function declaration*********************/
extern bool ErrM_GetFunctionPermission(ErrM_Func_ID functionID);
extern bool ErrM_SetErrorStatus(ErrM_Error_ID errorID, bool errorStatus);
extern void ErrM_Init(void);
extern void ErrM_mainFunction(void);

#endif /* _ERRM_HPP_ */
