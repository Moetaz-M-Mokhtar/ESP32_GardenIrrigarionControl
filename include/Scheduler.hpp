#ifndef _SPRINKLER_SCHEDULER_HPP_
#define _SPRINKLER_SCHEDULER_HPP_

#include <stdint.h>
#include <RTClib.h>
#include <HwAbstr.hpp>

#define SCHEDULER_FALLBACK_ALARM_SEC  (180 * 60)
#define SCHEDULER_FALLBACK_SLEEP_SEC  3600
#define SCHEDULER_MAX_ALARMS          15

class ScheduleAlarm
{
    private:
    uint8_t hours;
    uint8_t minutes;
    uint16_t period;
    uint8_t dow;
    uint8_t zones;      /* bitmask: bit0=zone0, bit1=zone1, etc. */
    HW_Driver* HW_Driver_Data;
    public:
    ScheduleAlarm(uint8_t h, uint8_t m, uint16_t period, uint8_t dow, uint8_t zones, HW_Driver* HW_Driver_Data);
    ~ScheduleAlarm(void);
    bool nextTriggerTime(uint32_t*);
    bool taskCompleteTime(uint32_t*);
    void evaluateAlarmState(void);

    /* getters */
    uint8_t getHours(void) const { return hours; }
    uint8_t getMinutes(void) const { return minutes; }
    uint16_t getPeriod(void) const { return period; }
    uint8_t getDow(void) const { return dow; }
    uint8_t getZones(void) const { return zones; }
    HW_Driver* getHwDriver(void) const { return HW_Driver_Data; }

    /* setters */
    void setHours(uint8_t h) { hours = h; }
    void setMinutes(uint8_t m) { minutes = m; }
    void setPeriod(uint16_t p) { period = p; }
    void setDow(uint8_t d) { dow = d; }
    void setZones(uint8_t z) { zones = z; }
    void setHwDriver(HW_Driver* d) { HW_Driver_Data = d; }
};

extern ScheduleAlarm ScheduleAlarm_arr[SCHEDULER_MAX_ALARMS];

void Scheduler_MainFunction(void);
uint32_t Scheduler_GetSecondsUntilNextAlarm(void);
bool Scheduler_IsDriverScheduledOn(uint8_t driverId);

/* alarm countdown display — for zone card timers in the app */
uint32_t Scheduler_GetAlarmTimerStart(uint8_t driverId);
uint32_t Scheduler_GetAlarmTimerDuration(uint8_t driverId);

#endif /* _SPRINKLER_SCHEDULER_HPP_ */
