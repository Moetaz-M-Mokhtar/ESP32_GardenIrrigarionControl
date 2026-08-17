#ifndef _COMMON_HPP_
#define _COMMON_HPP_

#include <stdint.h>

/****************************** system-wide constants *****************************/
#define COMMON_SERIAL_BAUDRATE          9600
#define COMMON_JSON_BUFFER_SIZE         1024
#define COMMON_MIN_ALARM_PERIOD_MIN     15
#define COMMON_SECONDS_PER_MINUTE       60
#define COMMON_BLE_STATUS_INTERVAL_SEC  1
#define COMMON_DEBUGM_DUMP_INTERVAL_SEC 10
#define COMMON_RTC_I2C_YEAR_MIN         2024
#define COMMON_SLEEP_FALLBACK_SEC       (180 * 60)

#endif /* _COMMON_HPP_ */
