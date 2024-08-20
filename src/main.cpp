#include "TimerCtrl.hpp"
#include "Scheduler.hpp"
#include "HwAbstr.hpp"

void setup () 
{
  HwAbstr_Init();
  TimerCtrl_Init();
}

void loop () 
{
  TimerCtrl_mainFunction();
  Scheduler_MainFunction();
  HwAbstr_GoToSleep();
}