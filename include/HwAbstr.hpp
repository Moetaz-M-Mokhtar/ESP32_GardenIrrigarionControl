#ifndef _HWABSTR_HPP_
#define _HWABSTR_HPP_

#include <stdint.h>
#include <HardwareSerial.h>
// #include <ArduinoJson.h>
//#include <SD.h>
// #include <SPI.h>
// #include <EEPROM.h>
#include <ErrM.hpp>

/*********************define*********************/

/*********************typeDef*********************/

/*********************global function declaration*********************/
void HwAbstr_Init(void);
void HwAbstr_MainFunction(void);

#endif /* _HWABSTR_HPP_ */