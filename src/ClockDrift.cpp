#include <ClockDrift.hpp>
#include <TimerCtrl.hpp>
#include <Preferences.h>
#include <Arduino.h>

/**************************************** define ***************************************/
#define NVS_NAMESPACE "clock_drift"
#define NVS_KEY_DRIFT_COEFF    "drift_ppm"
#define NVS_KEY_LAST_SYNC_TIME "last_sync_ts"

/********************************* local type definition *******************************/

/****************************** local variable declaration *****************************/
static Preferences nvsPrefs;
static float    driftPPM = 0.0f;
static uint32_t lastSyncTime = 0;

/******************************* local function declaration *****************************/

/******************************* local function definition *****************************/

/****************************** global function definition ****************************/

void ClockDrift_Init(void)
{
    nvsPrefs.begin(NVS_NAMESPACE, false);

    lastSyncTime = nvsPrefs.getUInt(NVS_KEY_LAST_SYNC_TIME, 0);
    driftPPM     = nvsPrefs.getFloat(NVS_KEY_DRIFT_COEFF, 0.0f);

    /* bound stale values written by older firmware */
    if (driftPPM >  CLOCKDRIFT_MAX_PPM)      driftPPM =  CLOCKDRIFT_MAX_PPM;
    else if (driftPPM < -CLOCKDRIFT_MAX_PPM) driftPPM = -CLOCKDRIFT_MAX_PPM;

    if (lastSyncTime > 0)
    {
        Serial.printf("ClockDrift: loaded drift=%.2f ppm, lastSync=%lu\n",
                       driftPPM, lastSyncTime);
    }
    else
    {
        Serial.println("ClockDrift: no previous sync, using raw RTC");
    }
}

DateTime ClockDrift_getCorrectedTime(void)
{
    DateTime raw = TimerCtrl_getCurrentTime();

    if (lastSyncTime == 0)
        return raw;

    uint32_t R = raw.unixtime();
    int32_t  elapsed = (int32_t)(R - lastSyncTime);
    float    correction = driftPPM * 1e-6f * (float)elapsed;

    return DateTime(R - (int32_t)correction);
}

DateTime ClockDrift_correctedToRaw(DateTime targetCorrected)
{
    if (lastSyncTime == 0)
        return targetCorrected;

    float p  = driftPPM * 1e-6f;
    float Ta = (float)targetCorrected.unixtime();
    float T0 = (float)lastSyncTime;

    uint32_t rawAlarm = (uint32_t)((Ta - p * T0) / (1.0f - p));
    return DateTime(rawAlarm);
}

void ClockDrift_syncRTC(uint32_t phoneUnixTime)
{
    /* capture raw RTC BEFORE adjustment */
    DateTime rawBefore = TimerCtrl_getCurrentTime();
    uint32_t rawBeforeUnix = rawBefore.unixtime();

    /* Step 1: read corrected time */
    DateTime corrected = ClockDrift_getCorrectedTime();
    uint32_t Tc = corrected.unixtime();

    /* Step 2: compute error (corrected - phone) */
    int32_t error = (int32_t)(Tc - phoneUnixTime);

    /* Step 3: compute elapsed since last sync */
    uint32_t elapsed = phoneUnixTime - lastSyncTime;

    /* Step 4: raw drift since last sync (raw RTC vs phone) */
    int32_t rawDrift = (int32_t)(rawBeforeUnix - phoneUnixTime);

    Serial.println("---- TimeSync BEGIN ----");
    Serial.printf("  phoneTime       = %lu\n", phoneUnixTime);
    Serial.printf("  rawBefore       = %lu (RTC before adjust)\n", rawBeforeUnix);
    Serial.printf("  correctedTime   = %lu (raw + drift correction)\n", Tc);
    Serial.printf("  lastSyncTime    = %lu\n", lastSyncTime);
    Serial.printf("  driftPPM (old)  = %.2f\n", driftPPM);
    Serial.printf("  elapsed         = %lu sec\n", elapsed);
    Serial.printf("  rawDrift        = %d sec (raw RTC - phone)\n", rawDrift);
    Serial.printf("  error           = %d sec (corrected - phone)\n", error);

    /* Step 5: update drift coefficient if calibration is valid */
    if (elapsed >= CLOCKDRIFT_MIN_CALIBRATION_INTERVAL &&
        abs(error) <= CLOCKDRIFT_MAX_CALIBRATION_ERROR)
    {
        float deltaP = (float)error / (float)elapsed * 1e6f;

        Serial.printf("  raw deltaP      = %.4f ppm\n", deltaP);

        /* safety clamp */
        if (deltaP > CLOCKDRIFT_MAX_PPM_STEP)
            deltaP = CLOCKDRIFT_MAX_PPM_STEP;
        else if (deltaP < -CLOCKDRIFT_MAX_PPM_STEP)
            deltaP = -CLOCKDRIFT_MAX_PPM_STEP;

        driftPPM += deltaP;

        /* hard ceiling — a corrupted measurement can never exceed ±3000 PPM */
        if (driftPPM >  CLOCKDRIFT_MAX_PPM)      driftPPM =  CLOCKDRIFT_MAX_PPM;
        else if (driftPPM < -CLOCKDRIFT_MAX_PPM) driftPPM = -CLOCKDRIFT_MAX_PPM;

        Serial.printf("  clamped deltaP  = %.2f ppm\n", deltaP);
        Serial.printf("  driftPPM (new)  = %.2f ppm\n", driftPPM);
        Serial.println("  calibration: ACCEPTED");
    }
    else
    {
        if (elapsed < CLOCKDRIFT_MIN_CALIBRATION_INTERVAL)
        {
            Serial.printf("  calibration: REJECTED (elapsed=%lu < %d sec)\n",
                           elapsed, CLOCKDRIFT_MIN_CALIBRATION_INTERVAL);
        }
        if (abs(error) > CLOCKDRIFT_MAX_CALIBRATION_ERROR)
        {
            Serial.printf("  calibration: REJECTED (|error|=%d > %d sec)\n",
                           abs(error), CLOCKDRIFT_MAX_CALIBRATION_ERROR);
        }
    }

    /* Step 6: always sync RTC to phone time */
    DateTime trueTime(phoneUnixTime);
    TimerCtrl_adjustTime(trueTime);

    /* verify RTC after adjustment */
    DateTime rawAfter = TimerCtrl_getCurrentTime();
    uint32_t rawAfterUnix = rawAfter.unixtime();
    int32_t residual = (int32_t)(rawAfterUnix - phoneUnixTime);

    Serial.printf("  RTC after adjust = %lu\n", rawAfterUnix);
    Serial.printf("  residual         = %d sec (RTC after - phone)\n", residual);

    /* Step 7: update sync reference */
    lastSyncTime = phoneUnixTime;

    nvsPrefs.putUInt(NVS_KEY_LAST_SYNC_TIME, lastSyncTime);
    nvsPrefs.putFloat(NVS_KEY_DRIFT_COEFF, driftPPM);

    Serial.printf("  saved lastSync   = %lu\n", lastSyncTime);
    Serial.printf("  saved driftPPM   = %.2f\n", driftPPM);
    Serial.println("---- TimeSync END ----");
}

void ClockDrift_resetDrift(void)
{
    driftPPM = 0.0f;
    lastSyncTime = 0;

    nvsPrefs.putFloat(NVS_KEY_DRIFT_COEFF, driftPPM);
    nvsPrefs.putUInt(NVS_KEY_LAST_SYNC_TIME, lastSyncTime);

    Serial.println("ClockDrift: drift reset (driftPPM=0, lastSyncTime=0)");
}

float ClockDrift_getCoeff(void)
{
    return driftPPM;
}

uint32_t ClockDrift_getLastSyncTime(void)
{
    return lastSyncTime;
}
