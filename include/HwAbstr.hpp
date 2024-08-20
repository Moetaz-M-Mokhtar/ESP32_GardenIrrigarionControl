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
void HwAbstr_GoToSleep(void);
void HWAbstr_updateGPIOPinState(gpio_num_t pinNumber, uint8_t state);

#endif /* _HWABSTR_HPP_ */