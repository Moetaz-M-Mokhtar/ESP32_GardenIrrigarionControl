#ifndef _CLOCKDRIFT_HPP_
#define _CLOCKDRIFT_HPP_

#include <stdint.h>
#include <RTClib.h>

/* calibration parameters (spec Section 6, 8) */
#define CLOCKDRIFT_MIN_CALIBRATION_INTERVAL  64800   /* 18 hours in seconds */
#define CLOCKDRIFT_MAX_CALIBRATION_ERROR     1800    /* 30 minutes in seconds */
#define CLOCKDRIFT_MAX_PPM_STEP              50.0f   /* safety clamp per sync */

void     ClockDrift_Init(void);
DateTime ClockDrift_getCorrectedTime(void);
DateTime ClockDrift_correctedToRaw(DateTime targetCorrected);
void     ClockDrift_syncRTC(uint32_t phoneUnixTime);
void     ClockDrift_resetDrift(void);
float    ClockDrift_getCoeff(void);
uint32_t ClockDrift_getLastSyncTime(void);

#endif /* _CLOCKDRIFT_HPP_ */
