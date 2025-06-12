/* include files */
#include <CfgM.hpp>
#include <TimerCtrl.hpp>
/**************************************** define ***************************************/

/********************************* local type definition *******************************/

/****************************** local variable definition *****************************/

/****************************** local function declaration *****************************/

/****************************** global variable definition *****************************/
HW_Driver_cfg HW_Driver_cfg_arr[] =
{
    HW_Driver_cfg(
        GPIO_NUM_15,                    //GPIO_Drive_pinNum
        LATCH_SN7475N_DRIVE,            //Solenoid_DriveType
        GPIO_NUM_14,                    //GPIO_Enable_pinNum
        1                               //coupled_HW_Driver_Idx
    ),
    HW_Driver_cfg(
        GPIO_NUM_13,                    //GPIO_Drive_pinNum
        LATCH_SN7475N_DRIVE,            //Solenoid_DriveType
        GPIO_NUM_14,                    //GPIO_Enable_pinNum
        0                               //coupled_HW_Driver_Idx
    ),
    HW_Driver_cfg(
        GPIO_NUM_15,                    //GPIO_Drive_pinNum
        LATCH_SN7475N_DRIVE,            //Solenoid_DriveType
        GPIO_NUM_27,                    //GPIO_Enable_pinNum
        0xFF                            //coupled_HW_Driver_Idx
    ),
};

ScheduleAlarm_cfg ScheduleAlarm_cfg_arr[] =
{
    ScheduleAlarm_cfg(
        7,                              //hours(24H)
        0,                              //minutes
        30,                             //period(minutes)
        0b11111100,                     //activeDoW(binary) Sunday -> Saturday
        true,                           //isEnabled (boolean)
        &HW_Driver_cfg_arr[0]           //HW_Driver_Data
    ),
    ScheduleAlarm_cfg(
        7,                              //hours(24H)
        30,                             //minutes
        30,                             //period(minutes)
        0b11111100,                     //activeDoW(binary) Sunday -> Saturday
        true,                           //isEnabled (boolean)
        &HW_Driver_cfg_arr[1]           //HW_Driver_Data
    ),
    ScheduleAlarm_cfg(
        7,                              //hours(24H)
        0,                              //minutes
        60,                             //period(minutes)
        0b11111110,                     //activeDoW(binary) Sunday -> Saturday
        true,                           //isEnabled (boolean)
        &HW_Driver_cfg_arr[2]           //HW_Driver_Data
    ),
    ScheduleAlarm_cfg(
        18,                             //hours(24H)
        0,                              //minutes
        30,                             //period(minutes)
        0b11111100,                     //activeDoW(binary) Sunday -> Saturday
        true,                           //isEnabled (boolean)
        &HW_Driver_cfg_arr[0]           //HW_Driver_Data
    ),
    ScheduleAlarm_cfg(
        18,                             //hours(24H)
        30,                             //minutes
        30,                             //period(minutes)
        0b11111100,                     //activeDoW(binary) Sunday -> Saturday
        true,                           //isEnabled (boolean)
        &HW_Driver_cfg_arr[1]           //HW_Driver_Data
    ),
    ScheduleAlarm_cfg(
        18,                             //hours(24H)
        0,                              //minutes
        60,                             //period(minutes)
        0b11111110,                     //activeDoW(binary) Sunday -> Saturday
        true,                           //isEnabled (boolean)
        &HW_Driver_cfg_arr[2]           //HW_Driver_Data
    )
};

/******************************* local function definition *****************************/


/****************************** global function definition ****************************/
HW_Driver_cfg::HW_Driver_cfg(uint8_t GPIO_Drive_pinNum, driveType_dt Solenoid_DriveType, uint8_t GPIO_Enable_pinNum, uint8_t coupled_HW_Driver_Idx)
                        :GPIO_Drive_pinNum(GPIO_Drive_pinNum), Solenoid_DriveType(Solenoid_DriveType), GPIO_Enable_pinNum(GPIO_Enable_pinNum), coupled_HW_Driver_Idx(coupled_HW_Driver_Idx)
{
    this->pin_OutputLevel = 0;
}

bool HW_Driver_cfg::set_HwState(uint8_t state)
{
    bool OpStatus = true; 
    if ((state != LOW) && (state != HIGH))
    {
        OpStatus = false;
    }
    else
    {
        this->pin_OutputLevel = state;
    }
    return OpStatus;
}

ScheduleAlarm_cfg::ScheduleAlarm_cfg(uint8_t h, uint8_t m, uint8_t period, uint8_t dow, uint8_t isEnabled, HW_Driver_cfg* HW_Driver_Data)
                                :hours(h), minutes(m), period(period), dow(dow), isEnabled(isEnabled), HW_Driver_Data(HW_Driver_Data)
{

}

ScheduleAlarm_cfg::~ScheduleAlarm_cfg()
{
    this->HW_Driver_Data->set_HwState(LOW);
}

bool ScheduleAlarm_cfg::nextTriggerTime(uint32_t* unix_time)
{
    bool OpStatus = false;
    DateTime currentTime = TimerCtrl_getCurrentTime();
    DateTime AlarmTime = DateTime(currentTime.year(), \
                                        currentTime.month(), \
                                        currentTime.day(), \
                                        this->hours, \
                                        this->minutes);
    *unix_time = 0xFFFFFFFF;

    if(this->isEnabled == true)
    {
        if(((this->dow >> (7 - AlarmTime.dayOfTheWeek()) & 0x01) == 1) &&
            (AlarmTime > currentTime))
        {
            *unix_time = AlarmTime.unixtime();
            OpStatus = true;
        }
        else
        {
            for(uint8_t loopIndex = 0; loopIndex < 7; loopIndex++)
            {
                AlarmTime = AlarmTime + *(new TimeSpan(1,0,0,0));
                if((this->dow >> (7 - AlarmTime.dayOfTheWeek()) & 0x01) == 1)
                {
                    *unix_time = AlarmTime.unixtime();
                    OpStatus = true;
                    break;
                }
            }
        }
    }
    return OpStatus;
}

bool ScheduleAlarm_cfg::taskCompleteTime(uint32_t* unix_time)
{
    bool OpStatus = false;

    if((this->isEnabled == true) && \
        (this->HW_Driver_Data->pin_OutputLevel == HIGH))
    {
        DateTime currentTime = TimerCtrl_getCurrentTime();
        DateTime AlarmTime = DateTime(currentTime.year(), \
                                        currentTime.month(), \
                                        currentTime.day(), \
                                        this->hours, \
                                        this->minutes);
        *unix_time = (AlarmTime + TimeSpan(this->period * 60)).unixtime();
        OpStatus = true;
    }

    return OpStatus;   
}

void ScheduleAlarm_cfg::evaluateAlarmState(void)
{
    uint8_t AlarmState = (LOW | this->HW_Driver_Data->pin_OutputLevel);

    if (this->isEnabled == true)
    {
        DateTime currentTime = TimerCtrl_getCurrentTime();

        if ((this->dow >> (7 - currentTime.dayOfTheWeek()) & 0x01) == 1)
        {
            DateTime alarmStartTime = DateTime(currentTime.year(), \
                                               currentTime.month(), \
                                               currentTime.day(), \
                                               this->hours, \
                                               this->minutes);
            DateTime alarmEndTime = alarmStartTime + TimeSpan(this->period * 60);
            if ((currentTime >= alarmStartTime) & (currentTime < alarmEndTime))
            {
                AlarmState = HIGH;
            }
            
        }
    }

    this->HW_Driver_Data->set_HwState(AlarmState);
}

void CfgM_Init(void)
{

}