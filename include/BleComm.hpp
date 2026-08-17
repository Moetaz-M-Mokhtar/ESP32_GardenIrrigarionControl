#ifndef _BLECOMM_HPP_
#define _BLECOMM_HPP_

#include <stdint.h>
#include <stdbool.h>

/* BLE UUIDs - must match Flutter app constants */
#define BLE_SERVICE_UUID             "12345678-1234-5678-1234-56789ABCDEF0"
#define BLE_TIMESYNC_CHAR_UUID       "ABCDEF00-1234-5678-1234-56789ABCDEF0"
#define BLE_CONFIG_CHAR_UUID         "ABCDEF01-1234-5678-1234-56789ABCDEF0"
#define BLE_STATUS_CHAR_UUID         "ABCDEF02-1234-5678-1234-56789ABCDEF0"
#define BLE_HEARTBEAT_CHAR_UUID      "ABCDEF04-1234-5678-1234-56789ABCDEF0"
#define BLE_SCHEDULES_CHAR_UUID      "ABCDEF06-1234-5678-1234-56789ABCDEF0"
#define BLE_FORCE_VALVE_CHAR_UUID    "ABCDEF07-1234-5678-1234-56789ABCDEF0"

/* timeout in seconds (compared against unix epoch) */
#define BLE_HEARTBEAT_TIMEOUT_SEC  30    /* 30 seconds */

#define BLE_DEVICE_NAME "SprinkCtrl"

void BleComm_Init(void);
void BleComm_mainFunction(void);
bool BleComm_isConnected(void);
bool BleComm_isPairingTimeout(void);
void BleComm_disconnect(void);
void BleComm_startAdvertising(void);
void BleComm_stopAdvertising(void);
bool BleComm_isPairingMode(void);
void BleComm_enterPairingMode(void);

#endif /* _BLECOMM_HPP_ */
