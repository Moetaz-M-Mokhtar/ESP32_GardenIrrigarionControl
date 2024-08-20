#ifndef _SPRINKLER_SCHEDULER_HPP_
#define _SPRINKLER_SCHEDULER_HPP_

#include <stdint.h>
#include <RTClib.h>

/*********************define*********************/
typedef struct
{
    gpio_num_t pinNumber;
} SprinklerDt;

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
    uint8_t isEnabled;
    uint8_t alarmState;
public:
    Scheduler_Alarm(uint8_t h, uint8_t m, uint8_t dow, uint8_t sprinklerIdx);
    uint8_t nextTriggerTime(uint32_t*);
    uint8_t taskCompleteTime(uint32_t*);
    void evaluateEventAction(DateTime currentTime);
    Scheduler_Alarm(uint8_t h, uint8_t m, uint8_t period, uint8_t dow, uint8_t sprinklerIdx, uint8_t isEnabled);
    ~Scheduler_Alarm(void);

    uint8_t getEventEnabled(void)
    {
        return this->isEnabled;
    }
};

void Scheduler_MainFunction(void);
/*********************global variable declaration*********************/

#endif /* _SPRINKLER_SCHEDULER_HPP_ */