#include <BleComm.hpp>
#include <ErrM.hpp>
#include <CfgM.hpp>
#include <Scheduler.hpp>
#include <TimerCtrl.hpp>
#include <ClockDrift.hpp>
#include <HwAbstr.hpp>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include <atomic>

/**************************************** define ***************************************/
#define BLE_DEVICE_NAME "SprinkCtrl"

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
static NimBLECharacteristic* pDriverConfigChar = nullptr;
static NimBLECharacteristic* pAlarmsChar = nullptr;
static NimBLECharacteristic* pForceValveChar = nullptr;

static std::atomic<BleComm_State> bleState{BLE_STATE_IDLE};
static std::atomic<uint32_t> lastHeartbeatTime{0};

/* time sync pending flag — NimBLE task sets, main loop processes */
static volatile bool timeSyncPending = false;
static volatile uint32_t pendingSyncTime = 0;

/* status update throttle (in seconds, unix epoch) */
#define BLE_STATUS_UPDATE_INTERVAL_SEC 1
static uint32_t lastStatusUpdateTime = 0;
static volatile bool alarmsNeedRefresh = false;

/* JSON serialization buffers — separate for status and alarms to avoid race */
static char jsonBuffer[1024];
static char alarmJsonBuffer[1024];

/* pairing mode state — entered on normal reset, 30s window for first BLE connect */
static bool pairingMode = false;
static uint32_t pairingStartTime = 0;

/******************************* local function declaration *****************************/
static void BleComm_updateStatus(void);
static void BleComm_handleTimeSyncWrite(const uint8_t* data, size_t len);
static void BleComm_handleConfigWrite(const uint8_t* data, size_t len);
static void BleComm_handleForceValveWrite(const uint8_t* data, size_t len);
static void BleComm_handleDriverConfigWrite(const uint8_t* data, size_t len);
static void BleComm_updateAlarmsInternal(void);
static void BleComm_disconnectClient(void);

/******************************* local function definition *****************************/

/* BLE Server Callbacks */
class BleCommServerCallbacks : public NimBLEServerCallbacks
{
    void onConnect(NimBLEServer* pServer) override
    {
        lastHeartbeatTime = ClockDrift_getCorrectedTime().unixtime();
        bleState = BLE_STATE_CONNECTED;
        Serial.println("BLE: Client connected");
    }

    void onDisconnect(NimBLEServer* pServer) override
    {
        bleState = BLE_STATE_IDLE;
        Serial.println("BLE: Client disconnected");
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
                    Serial.println("BLE: Client requested disconnect");
                    BleComm_disconnectClient();
                }
            }
        }
    }
};

class BleCommDriverConfigCallback : public NimBLECharacteristicCallbacks
{
    void onWrite(NimBLECharacteristic* pCharacteristic) override
    {
        auto value = pCharacteristic->getValue();
        BleComm_handleDriverConfigWrite(
            reinterpret_cast<const uint8_t*>(value.data()),
            value.length());
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
static BleCommDriverConfigCallback driverConfigCallbacks;
static BleCommForceValveCallback forceValveCallbacks;

static void BleComm_disconnectClient(void)
{
    if (pServer != nullptr && pServer->getConnectedCount() > 0)
    {
        NimBLEAddress addr = pServer->getPeerInfo(0).getAddress();
        Serial.printf("BLE: Disconnecting client %s\n", addr.toString().c_str());
        pServer->disconnect(addr);
    }
}

/****************************** global function definition ****************************/

void BleComm_Init(void)
{
    if (!ErrM_GetFunctionPermission(ERRM_FUNC_SPCONN))
    {
        Serial.println("BLE: SPCONN inhibited, skipping init");
        return;
    }

    Serial.println("BLE: Initializing...");

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

    /* Driver Config Characteristic (Read/Write) */
    pDriverConfigChar = pService->createCharacteristic(
        BLE_DRIVER_CONFIG_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
    );
    pDriverConfigChar->setCallbacks(&driverConfigCallbacks);

    /* Alarms Characteristic (Read/Notify) */
    pAlarmsChar = pService->createCharacteristic(
        BLE_ALARMS_CHAR_UUID,
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
    Serial.printf("BLE: Initialized and advertising (MAC: %s)\n",
                  addr.toString().c_str());

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
            Serial.println("BLE: Heartbeat timeout, disconnecting");
            BleComm_disconnect();
            return;
        }

        /* process pending time sync — always, regardless of status interval.
           Forces immediate status update so Flutter reads the correct time. */
        if (timeSyncPending)
        {
            timeSyncPending = false;
            uint32_t raw_before = TimerCtrl_getCurrentTime().unixtime();
            Serial.printf("TimeSync PROCESS: raw_before=%lu phone=%lu delta=%+dsec\n",
                           raw_before, pendingSyncTime, (int32_t)(pendingSyncTime - raw_before));
            ClockDrift_syncRTC(pendingSyncTime);
            uint32_t raw_after = TimerCtrl_getCurrentTime().unixtime();
            uint32_t corrected = ClockDrift_getCorrectedTime().unixtime();
            Serial.printf("TimeSync DONE: raw=%lu corrected=%lu phone=%lu residual=%+dsec\n",
                           raw_after, corrected, pendingSyncTime,
                           (int32_t)(raw_after - pendingSyncTime));

            /* force immediate status update — Flutter reads right after sync */
            BleComm_updateStatus();
            lastStatusUpdateTime = ClockDrift_getCorrectedTime().unixtime();
            return;
        }

        /* process alarm read request immediately — every loop iteration.
           Must NOT be gated behind the status interval timer: Flutter sends
           read_alarms then reads the response back after 200ms. Gating the
           flag behind the 1s status tick meant the read-back returned stale
           data until the next tick (or until a second action forced it). */
        if (alarmsNeedRefresh)
        {
            alarmsNeedRefresh = false;
            BleComm_updateAlarmsInternal();
        }

        /* update status periodically */
        if ((now - lastStatusUpdateTime) >= BLE_STATUS_UPDATE_INTERVAL_SEC)
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

    uint32_t correctedUnix = corrected.unixtime();

    /* build status JSON */
    JsonDocument doc;
    doc["unix"] = correctedUnix;
    doc["temp"] = temp;
    doc["boot"] = HwAbstr_GetBootCount();
    doc["drift_ppm"] = drift;
    doc["last_sync"] = lastSync;
    doc["mode"] = "automatic";
    doc["pairing"] = BleComm_isPairingMode();

    /* valve states - 4 valves. Report EFFECTIVE state:
       forced state always wins, then scheduler-intended
       state (an alarm is currently within its active window). */
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

    /* driver enabled states */
    JsonArray drivers = doc["drivers"].to<JsonArray>();
    for (uint8_t i = 0; i < HWABSTR_MAX_DRIVERS; i++)
    {
        drivers.add(CfgM_IsDriverEnabled(i) ? 1 : 0);
    }

    /* timer arrays - for countdown display */
    JsonArray timerStarts = doc["timer_start"].to<JsonArray>();
    JsonArray timerDurations = doc["timer_duration"].to<JsonArray>();
    for (uint8_t i = 0; i < HWABSTR_MAX_DRIVERS; i++)
    {
        timerStarts.add(Scheduler_GetAlarmTimerStart(i));
        timerDurations.add(Scheduler_GetAlarmTimerDuration(i));
    }

    /* errors - send actual error IDs */
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

static void BleComm_updateAlarmsInternal(void)
{
    if (pAlarmsChar == nullptr) return;

    /*
     * Flat array format: {"a":[[id,h,m,period,dow,zones,drv],...]}
     * Only sends alarms with dow != 0 (used slots).
     * Flutter infers free slots from missing IDs.
     */
    JsonDocument doc;
    JsonArray alarms = doc["a"].to<JsonArray>();
    uint8_t usedCount = 0;
    for (uint8_t i = 0; i < SCHEDULER_MAX_ALARMS; i++)
    {
        if (ScheduleAlarm_arr[i].getDow() == 0) continue;

        JsonArray a = alarms.add<JsonArray>();
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
        usedCount++;
    }

    size_t len = serializeJson(doc, alarmJsonBuffer, sizeof(alarmJsonBuffer));
    if (len >= sizeof(alarmJsonBuffer))
    {
        Serial.println("BLE: Alarms JSON truncated");
    }
    pAlarmsChar->setValue(reinterpret_cast<const uint8_t*>(alarmJsonBuffer), len);
    if (BleComm_isConnected())
    {
        pAlarmsChar->notify();
    }
}

static void BleComm_handleTimeSyncWrite(const uint8_t* data, size_t len)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error)
    {
        Serial.printf("BLE TimeSync: JSON parse error: %s\n", error.c_str());
        return;
    }

    if (!doc["ts"].is<uint32_t>())
    {
        Serial.println("BLE TimeSync: missing 'ts' field");
        return;
    }

    uint32_t newTime = doc["ts"].as<uint32_t>();
    uint32_t rawNow = TimerCtrl_getCurrentTime().unixtime();
    int32_t bleDelta = (int32_t)(newTime - rawNow);

    Serial.printf("BLE TimeSync RX: phone=%lu rawNow=%lu bleDelta=%+dsec\n",
                   newTime, rawNow, bleDelta);

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
        Serial.printf("BLE Config: JSON parse error: %s\n", error.c_str());
        return;
    }

    /* handle read_alarms request — flag for main loop to serialize */
    if (doc["action"].is<const char*>() &&
        strcmp(doc["action"].as<const char*>(), "read_alarms") == 0)
    {
        alarmsNeedRefresh = true;
        Serial.println("BLE Config: read_alarms request queued");
        return;
    }

    /* handle reset_drift request — clear calibration, force status update */
    if (doc["action"].is<const char*>() &&
        strcmp(doc["action"].as<const char*>(), "reset_drift") == 0)
    {
        ClockDrift_resetDrift();
        BleComm_updateStatus();
        Serial.println("BLE Config: reset_drift processed");
        return;
    }

    if (!doc["alarm"].is<JsonObject>())
    {
        Serial.println("BLE Config: missing 'alarm' field");
        return;
    }

    JsonObject alarm = doc["alarm"];
    uint8_t id = alarm["id"].as<uint8_t>();
    uint8_t h = alarm["h"].as<uint8_t>();
    uint8_t m = alarm["m"].as<uint8_t>();
    uint16_t period = alarm["period"].as<uint16_t>();
    uint8_t dow = alarm["dow"].as<uint8_t>();
    uint8_t zones = alarm["zones"].as<uint8_t>();

    Serial.printf("BLE Config: alarm %d -> %02d:%02d, period=%d, dow=0x%02X, zones=0x%02X\n",
                   id, h, m, period, dow, zones);

    if (!CfgM_SetAlarm(id, h, m, period, dow, zones))
    {
        Serial.printf("BLE Config: SetAlarm failed for id %d\n", id);
        return;
    }

    /* optional: reassign driver */
    if (alarm["driver"].is<uint8_t>())
    {
        uint8_t driverId = alarm["driver"].as<uint8_t>();
        CfgM_SetAlarmDriver(id, driverId);
    }
}

static void BleComm_handleDriverConfigWrite(const uint8_t* data, size_t len)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error)
    {
        Serial.printf("BLE DriverConfig: JSON parse error: %s\n", error.c_str());
        return;
    }

    if (!doc["driver"].is<uint8_t>() || !doc["enabled"].is<bool>())
    {
        Serial.println("BLE DriverConfig: missing 'driver' or 'enabled' field");
        return;
    }

    uint8_t driverId = doc["driver"].as<uint8_t>();
    bool enabled = doc["enabled"].as<bool>();

    if (driverId >= HWABSTR_MAX_DRIVERS)
    {
        Serial.printf("BLE DriverConfig: invalid driver ID %d\n", driverId);
        return;
    }

    /* if disabling, turn off the valve and clear force state */
    if (!enabled)
    {
        /* clear force state so device can sleep */
        if (HW_Driver_arr[driverId].forced)
        {
            HwAbstr_clearForce(driverId);
        }
    }

    CfgM_SetDriverEnabled(driverId, enabled);
    Serial.printf("BLE DriverConfig: driver %d %s\n", driverId, enabled ? "enabled" : "disabled");
}

static void BleComm_handleForceValveWrite(const uint8_t* data, size_t len)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error)
    {
        Serial.printf("BLE ForceValve: JSON parse error: %s\n", error.c_str());
        return;
    }

    if (!doc["valve"].is<uint8_t>() || !doc["force"].is<bool>() || !doc["state"].is<uint8_t>())
    {
        Serial.println("BLE ForceValve: missing 'valve', 'force', or 'state' field");
        return;
    }

    uint8_t valve = doc["valve"].as<uint8_t>();
    bool force = doc["force"].as<bool>();
    uint8_t state = doc["state"].as<uint8_t>();
    uint32_t duration = doc["duration"].as<uint32_t>();

    if (valve >= HWABSTR_MAX_DRIVERS)
    {
        Serial.printf("BLE ForceValve: invalid valve index %d\n", valve);
        return;
    }

    if (!CfgM_IsDriverEnabled(valve))
    {
        Serial.printf("BLE ForceValve: driver %d is disabled\n", valve);
        return;
    }

    /* delegate to HwAbstr — force state, timer, and GPIO override all managed there */
    if (force)
    {
        HwAbstr_setForce(valve, state, duration);
    }
    else
    {
        HwAbstr_clearForce(valve);
    }

    Serial.printf("BLE ForceValve: valve %d -> %s (force=%d, duration=%lu)\n",
                   valve, state ? "ON" : "OFF", force, duration);
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
        Serial.println("BleComm: Entering pairing mode");
    }
}

bool BleComm_isPairingTimeout(void)
{
    if (!pairingMode) return true;
    uint32_t elapsed = (ClockDrift_getCorrectedTime().unixtime() - pairingStartTime);
    return (elapsed >= HWABSTR_PAIRING_TIMEOUT_SEC);
}
