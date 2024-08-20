/* include files */
#include <CfgM.hpp>
#include <ErrM.hpp>
#include <HwAbstr.hpp>
#include <ArduinoJson.h>

/**************************************** define ***************************************/

/********************************* local type definition *******************************/

/****************************** local variable definition *****************************/

/****************************** global variable definition *****************************/
JsonArray CFGM_sprinklerCfg;
JsonArray CFGM_scheduleCfg;

/******************************* local function declaration *****************************/

/****************************** local function definition *****************************/

/****************************** global function declaration ****************************/
void CfgM_Init(void)
{
    // JsonDocument CfgFile;
    
    // if(HwAbstr_GetCfgString(&CfgFile) == true)
    // {
    //     /*************************FAIL POINT*******************************/
    //     /* missing checks for configuration format validity
    //     /******************************************************************/ 
    //     CFGM_sprinklerCfg = CfgFile["sprinkler"].to<JsonArray>();
    //     CFGM_scheduleCfg  = CfgFile["scheduler"].to<JsonArray>();
    // }
    // else
    // {
    //     //no Cfgs available
    // }
}

void LoadTestCfgs()
{
    
}