#ifndef _SPRINKLER_SCHEDULER_HPP_
#define _SPRINKLER_SCHEDULER_HPP_

#include <stdint.h>
#include<RTClib.h>

/*********************define*********************/

/*********************typeDef*********************/
class Scheduler_Alarm
{
private:
    /* data */
    uint8_t hours;
    uint8_t minutes;
    uint8_t dow;
    uint8_t sprinklerIdx;
    uint8_t period;
    uint8_t isActive;
public:
    Scheduler_Alarm(uint8_t h, uint8_t m, uint8_t dow, uint8_t sprinklerIdx);
    uint32_t timeToNextTrigger(void);
    void evaluateEventAction(DateTime currentTime);

    Scheduler_Alarm(uint8_t h, uint8_t m, uint8_t period, uint8_t dow, uint8_t sprinklerIdx, uint8_t isActive)
    {
        this->hours = h;
        this->minutes = m;
        this->sprinklerIdx = sprinklerIdx;
        this->isActive = isActive;
        this->dow = dow;
        this->period = period;
    }

    ~Scheduler_Alarm(void)
    {
        
    }

    uint8_t isEventActive(void)
    {
        return this->isActive;
    }
};

/*********************global variable declaration*********************/
extern Scheduler_Alarm alarmObj[3];

#endif /* _SPRINKLER_SCHEDULER_HPP_ */