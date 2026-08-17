#include <BleComm.hpp>
#include <ErrM.hpp>
#include <CfgM.hpp>
#include <Scheduler.hpp>
#include <TimerCtrl.hpp>
#include <ClockDrift.hpp>
#include <HwAbstr.hpp>
#include <Common.hpp>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include <atomic>

/********************************* local type definition *******************************/
enum BleComm_State
{
    BLE_STATE_IDLE = 0,
    BLE_STATE_CONNECTED
};

/****************************** local variable declaration *****************************/
static NimBLEServer* pServer = nullptr;
static NimBLEService* pService = nullptr;
static NimBLECharacteristic* pTimeSyncChar = nullptr;
static NimBLECharacteristic* pConfigChar = nullptr;
static NimBLECharacteristic* pStatusChar = nullptr;
static NimBLECharacteristic* pHeartbeatChar = nullptr;
static NimBLECharacteristic* pSchedulesChar = nullptr;
static NimBLECharacteristic* pForceValveChar = nullptr;

static std::atomic<BleComm_State> bleState{BLE_STATE_IDLE};
static std::atomic<uint32_t> lastHeartbeatTime{0};

/* time sync pending flag — NimBLE task sets, main loop processes */
static volatile bool timeSyncPending = false;
static volatile uint32_t pendingSyncTime = 0;

/* status update throttle */
static uint32_t lastStatusUpdateTime = 0;
static volatile bool schedulesNeedRefresh = false;

/* JSON serialization buffers — separate for status and schedules to avoid race */
static char jsonBuffer[COMMON_JSON_BUFFER_SIZE];
static char scheduleJsonBuffer[COMMON_JSON_BUFFER_SIZE];

/* pairing mode state — entered on non-RTC wake, 30s window for first BLE connect */
static bool pairingMode = false;
static uint32_t pairingStartTime = 0;

/******************************* local function declaration *****************************/
static void BleComm_updateStatus(void);
static void BleComm_handleTimeSyncWrite(const uint8_t* data, size_t len);
static void BleComm_handleConfigWrite(const uint8_t* data, size_t len);
static void BleComm_handleForceValveWrite(const uint8_t* data, size_t len);
static void BleComm_updateSchedulesInternal(void);
static void BleComm_disconnectClient(void);

/******************************* local function definition *****************************/

/* BLE Server Callbacks */
class BleCommServerCallbacks : public NimBLEServerCallbacks
{
    void onConnect(NimBLEServer* pServer) override
    {
        lastHeartbeatTime = ClockDrift_getCorrectedTime().unixtime();
        bleState = BLE_STATE_CONNECTED;
    }

    void onDisconnect(NimBLEServer* pServer) override
    {
        bleState = BLE_STATE_IDLE;
    }
};

/* Characteristic Callbacks */
class BleCommTimeSyncCallback : public NimBLECharacteristicCallbacks
{
    void onWrite(NimBLECharacteristic* pCharacteristic) override
    {
        auto value = pCharacteristic->getValue();
        BleComm_handleTimeSyncWrite(
            reinterpret_cast<const uint8_t*>(value.data()),
            value.length());
    }
};

class BleCommConfigCallback : public NimBLECharacteristicCallbacks
{
    void onWrite(NimBLECharacteristic* pCharacteristic) override
    {
        auto value = pCharacteristic->getValue();
        BleComm_handleConfigWrite(
            reinterpret_cast<const uint8_t*>(value.data()),
            value.length());
    }
};

class BleCommHeartbeatCallback : public NimBLECharacteristicCallbacks
{
    void onWrite(NimBLECharacteristic* pCharacteristic) override
    {
        lastHeartbeatTime = ClockDrift_getCorrectedTime().unixtime();

        auto value = pCharacteristic->getValue();
        if (value.length() > 0)
        {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, value.data(), value.length());
            if (!err && doc["action"].is<const char*>())
            {
                const char* action = doc["action"];
                if (strcmp(action, "disconnect") == 0)
                {
                    BleComm_disconnectClient();
                }
            }
        }
    }
};

class BleCommForceValveCallback : public NimBLECharacteristicCallbacks
{
    void onWrite(NimBLECharacteristic* pCharacteristic) override
    {
        auto value = pCharacteristic->getValue();
        BleComm_handleForceValveWrite(
            reinterpret_cast<const uint8_t*>(value.data()),
            value.length());
    }
};

/* static callback instances */
static BleCommServerCallbacks serverCallbacks;
static BleCommTimeSyncCallback timeSyncCallbacks;
static BleCommConfigCallback configCallbacks;
static BleCommHeartbeatCallback heartbeatCallbacks;
static BleCommForceValveCallback forceValveCallbacks;

static void BleComm_disconnectClient(void)
{
    if (pServer != nullptr && pServer->getConnectedCount() > 0)
    {
        NimBLEAddress addr = pServer->getPeerInfo(0).getAddress();
        pServer->disconnect(addr);
    }
}

/****************************** global function definition ****************************/

void BleComm_Init(void)
{
    if (!ErrM_GetFunctionPermission(ERRM_FUNC_SPCONN))
    {
        return;
    }

    /* Initialize BLE device */
    NimBLEDevice::init(BLE_DEVICE_NAME);

    /* Security: no bonding required for simple config app */
    NimBLEDevice::setSecurityAuth(false, false, false);

    /* Create server */
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&serverCallbacks);

    /* Create service */
    pService = pServer->createService(BLE_SERVICE_UUID);

    /* Time Sync Characteristic (Read/Write) */
    pTimeSyncChar = pService->createCharacteristic(
        BLE_TIMESYNC_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
    );
    pTimeSyncChar->setCallbacks(&timeSyncCallbacks);

    /* Config Characteristic (Read/Write) */
    pConfigChar = pService->createCharacteristic(
        BLE_CONFIG_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
    );
    pConfigChar->setCallbacks(&configCallbacks);

    /* Status Characteristic (Read/Notify) */
    pStatusChar = pService->createCharacteristic(
        BLE_STATUS_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    /* Heartbeat Characteristic (Write only) */
    pHeartbeatChar = pService->createCharacteristic(
        BLE_HEARTBEAT_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    pHeartbeatChar->setCallbacks(&heartbeatCallbacks);

    /* Schedules Characteristic (Read/Notify) */
    pSchedulesChar = pService->createCharacteristic(
        BLE_SCHEDULES_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    /* Force Valve Characteristic (Read/Write) */
    pForceValveChar = pService->createCharacteristic(
        BLE_FORCE_VALVE_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
    );
    pForceValveChar->setCallbacks(&forceValveCallbacks);

    /* Start service */
    pService->start();

    /* Configure advertising */
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);
    BleComm_startAdvertising();

    NimBLEAddress addr = NimBLEDevice::getAddress();
    Serial.printf("BLE: init OK MAC=%s\n", addr.toString().c_str());

    BleComm_enterPairingMode();
}

void BleComm_mainFunction(void)
{
    if (!ErrM_GetFunctionPermission(ERRM_FUNC_SPCONN))
    {
        return;
    }

    if (bleState == BLE_STATE_CONNECTED)
    {
        /* check heartbeat timeout */
        uint32_t now = ClockDrift_getCorrectedTime().unixtime();
        if ((now - lastHeartbeatTime) > BLE_HEARTBEAT_TIMEOUT_SEC)
        {
            BleComm_disconnect();
            return;
        }

        /* process pending time sync */
        if (timeSyncPending)
        {
            timeSyncPending = false;
            ClockDrift_syncRTC(pendingSyncTime);

            /* force immediate status update */
            BleComm_updateStatus();
            lastStatusUpdateTime = ClockDrift_getCorrectedTime().unixtime();
            return;
        }

        /* process schedule read request immediately */
        if (schedulesNeedRefresh)
        {
            schedulesNeedRefresh = false;
            BleComm_updateSchedulesInternal();
        }

        /* update status periodically */
        if ((now - lastStatusUpdateTime) >= COMMON_BLE_STATUS_INTERVAL_SEC)
        {
            BleComm_updateStatus();
            lastStatusUpdateTime = now;
        }
    }
}

bool BleComm_isConnected(void)
{
    return (bleState != BLE_STATE_IDLE);
}

void BleComm_disconnect(void)
{
    BleComm_disconnectClient();
}

/****************************** local function definition ****************************/

static void BleComm_updateStatus(void)
{
    if (pStatusChar == nullptr)
    {
        return;
    }

    DateTime corrected = ClockDrift_getCorrectedTime();
    float temp = TimerCtrl_getTemperature();
    float drift = ClockDrift_getCoeff();
    uint32_t lastSync = ClockDrift_getLastSyncTime();

    /* build status JSON */
    JsonDocument doc;
    doc["unix"] = corrected.unixtime();
    doc["temp"] = temp;
    doc["boot"] = HwAbstr_GetBootCount();
    doc["drift_ppm"] = drift;
    doc["last_sync"] = lastSync;
    doc["mode"] = "automatic";

    /* valve states — report EFFECTIVE state:
       forced state always wins, then scheduler-intended state */
    JsonArray valves = doc["valves"].to<JsonArray>();
    for (uint8_t i = 0; i < HWABSTR_MAX_DRIVERS; i++)
    {
        bool effectiveOn;
        if (HW_Driver_arr[i].forced)
        {
            effectiveOn = (HW_Driver_arr[i].forcedState == 1);
        }
        else
        {
            effectiveOn = Scheduler_IsDriverScheduledOn(i);
        }
        valves.add(effectiveOn ? 1 : 0);
    }

    /* forced states */
    JsonArray forced = doc["forced"].to<JsonArray>();
    for (uint8_t i = 0; i < HWABSTR_MAX_DRIVERS; i++)
    {
        forced.add(HW_Driver_arr[i].forced ? 1 : 0);
    }

    /* timer arrays — for countdown display */
    JsonArray timerStarts = doc["timer_start"].to<JsonArray>();
    JsonArray timerDurations = doc["timer_duration"].to<JsonArray>();
    for (uint8_t i = 0; i < HWABSTR_MAX_DRIVERS; i++)
    {
        timerStarts.add(Scheduler_GetAlarmTimerStart(i));
        timerDurations.add(Scheduler_GetAlarmTimerDuration(i));
    }

    /* errors — send actual error IDs */
    JsonArray errors = doc["errors"].to<JsonArray>();
    for (uint8_t i = 1; i < ERRM_ERROR_COUNT; i++)
    {
        if (ErrM_GetErrorStatus((ErrM_Error_ID)i) == true)
        {
            errors.add(i);
        }
    }

    size_t len = serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
    if (len >= sizeof(jsonBuffer))
    {
        Serial.println("BLE: Status JSON truncated");
    }
    pStatusChar->setValue(reinterpret_cast<const uint8_t*>(jsonBuffer), len);
    pStatusChar->notify();
}

static void BleComm_updateSchedulesInternal(void)
{
    if (pSchedulesChar == nullptr) return;

    /*
     * Flat array format: {"s":[[id,h,m,period,dow,zones,drv],...]}
     * Only sends schedules with dow != 0 (used slots).
     */
    JsonDocument doc;
    JsonArray schedules = doc["s"].to<JsonArray>();
    for (uint8_t i = 0; i < SCHEDULER_MAX_ALARMS; i++)
    {
        if (ScheduleAlarm_arr[i].getDow() == 0) continue;

        JsonArray a = schedules.add<JsonArray>();
        a.add(i);
        a.add(ScheduleAlarm_arr[i].getHours());
        a.add(ScheduleAlarm_arr[i].getMinutes());
        a.add(ScheduleAlarm_arr[i].getPeriod());
        a.add(ScheduleAlarm_arr[i].getDow());
        a.add(ScheduleAlarm_arr[i].getZones());

        HW_Driver* hw = ScheduleAlarm_arr[i].getHwDriver();
        uint8_t driverIdx = 0xFF;
        for (uint8_t d = 0; d < HWABSTR_MAX_DRIVERS; d++)
        {
            if (hw == &HW_Driver_arr[d])
            {
                driverIdx = d;
                break;
            }
        }
        a.add(driverIdx);
    }

    size_t len = serializeJson(doc, scheduleJsonBuffer, sizeof(scheduleJsonBuffer));
    if (len >= sizeof(scheduleJsonBuffer))
    {
        Serial.println("BLE: Schedules JSON truncated");
    }
    pSchedulesChar->setValue(reinterpret_cast<const uint8_t*>(scheduleJsonBuffer), len);
    if (BleComm_isConnected())
    {
        pSchedulesChar->notify();
    }
}

static void BleComm_handleTimeSyncWrite(const uint8_t* data, size_t len)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error || !doc["ts"].is<uint32_t>())
    {
        return;
    }

    uint32_t newTime = doc["ts"].as<uint32_t>();

    /* defer to main loop — avoids I2C race from NimBLE task */
    pendingSyncTime = newTime;
    timeSyncPending = true;
}

static void BleComm_handleConfigWrite(const uint8_t* data, size_t len)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error)
    {
        return;
    }

    /* handle read_schedules request */
    if (doc["action"].is<const char*>() &&
        strcmp(doc["action"].as<const char*>(), "read_schedules") == 0)
    {
        schedulesNeedRefresh = true;
        return;
    }

    /* handle reset_drift request */
    if (doc["action"].is<const char*>() &&
        strcmp(doc["action"].as<const char*>(), "reset_drift") == 0)
    {
        ClockDrift_resetDrift();
        BleComm_updateStatus();
        return;
    }

    if (!doc["schedule"].is<JsonObject>())
    {
        return;
    }

    JsonObject schedule = doc["schedule"];
    uint8_t id = schedule["id"].as<uint8_t>();
    uint8_t h = schedule["h"].as<uint8_t>();
    uint8_t m = schedule["m"].as<uint8_t>();
    uint16_t period = schedule["period"].as<uint16_t>();
    uint8_t dow = schedule["dow"].as<uint8_t>();
    uint8_t zones = schedule["zones"].as<uint8_t>();

    if (!CfgM_SetAlarm(id, h, m, period, dow, zones))
    {
        return;
    }

    /* optional: reassign driver */
    if (schedule["driver"].is<uint8_t>())
    {
        uint8_t driverId = schedule["driver"].as<uint8_t>();
        CfgM_SetAlarmDriver(id, driverId);
    }
}

static void BleComm_handleForceValveWrite(const uint8_t* data, size_t len)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error)
    {
        return;
    }

    if (!doc["valve"].is<uint8_t>() || !doc["force"].is<bool>() || !doc["state"].is<uint8_t>())
    {
        return;
    }

    uint8_t valve = doc["valve"].as<uint8_t>();
    bool force = doc["force"].as<bool>();
    uint8_t state = doc["state"].as<uint8_t>();
    uint32_t duration = doc["duration"].as<uint32_t>();

    if (valve >= HWABSTR_MAX_DRIVERS)
    {
        return;
    }

    if (force)
    {
        HwAbstr_setForce(valve, state, duration);
    }
    else
    {
        HwAbstr_clearForce(valve);
    }
}

void BleComm_startAdvertising(void)
{
    if (!NimBLEDevice::startAdvertising())
    {
        ErrM_SetErrorStatus(ERRM_BLE_ADVERTISING_FAILED, true);
    }
    else
    {
        ErrM_SetErrorStatus(ERRM_BLE_ADVERTISING_FAILED, false);
    }
}

void BleComm_stopAdvertising(void)
{
    NimBLEDevice::stopAdvertising();
}

bool BleComm_isPairingMode(void)
{
    return pairingMode;
}

void BleComm_enterPairingMode(void)
{
    if (!pairingMode)
    {
        pairingMode = true;
        pairingStartTime = ClockDrift_getCorrectedTime().unixtime();
    }
}

bool BleComm_isPairingTimeout(void)
{
    if (!pairingMode) return true;
    uint32_t elapsed = (ClockDrift_getCorrectedTime().unixtime() - pairingStartTime);
    return (elapsed >= HWABSTR_PAIRING_TIMEOUT_SEC);
}
