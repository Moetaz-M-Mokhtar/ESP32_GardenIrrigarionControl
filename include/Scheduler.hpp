#ifndef _SPRINKLER_SCHEDULER_HPP_
#define _SPRINKLER_SCHEDULER_HPP_

#include <stdint.h>
#include <RTClib.h>

#define SCHEDULER_FALLBACK_ALARM_SEC  (180 * 60)
#define SCHEDULER_FALLBACK_SLEEP_SEC  3600

void Scheduler_MainFunction(void);
uint32_t Scheduler_GetSecondsUntilNextAlarm(void);
bool Scheduler_IsDriverScheduledOn(uint8_t driverId);

#endif /* _SPRINKLER_SCHEDULER_HPP_ */
