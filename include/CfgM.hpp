#ifndef _CFGM_HPP_
#define _CFGM_HPP_

#include <stdint.h>
#include <RTClib.h>

/*********************Cfg types definition*********************/
class sprinklerCfgNode {  
public: 
    uint8_t sprinklerID;
    uint8_t pin; 
    sprinklerCfgNode* next; 

    // Parameterised Constructor 
    sprinklerCfgNode(uint8_t ID, uint8_t pin) 
    {
        this->sprinklerID = ID; 
        this->pin = pin; 
        this->next = NULL; 
    } 
};

class scheduleCfgNode {  
public:
    DateTime scheduleTime;
    bool active_dayOfWeek[7];
    uint8_t sprinklerID;
    scheduleCfgNode* next;

    // Parameterised Constructor 
    scheduleCfgNode(DateTime scheduleTime, bool active_dayOfWeek[7], uint8_t sprinklerID) 
    { 
        this->scheduleTime = scheduleTime;
        for(uint8_t loopIndex = 0; loopIndex < 7; loopIndex++)
        {
            this->active_dayOfWeek[loopIndex] = active_dayOfWeek[loopIndex];
        }
        this->sprinklerID = sprinklerID;
        this->next = NULL; 
    } 
}; 
  
// Linked list class to 
// implement a linked list. 
template <typename T>
class Linkedlist { 
private:
    T* head; 
    T* tail;
  
public: 
    // Default constructor 
    Linkedlist()
    {
        head = NULL;
        tail = NULL;
    } 
  
    // Function to insert a 
    // node at the end of the 
    // linked list. 
    void addNode(T* nodePtr)
    {
        T* tempPtr;

        if((this->head == NULL) && (this->tail == NULL))
        {
            this->head.nextPtr = nodePtr;
            this->tail.nextPtr = nodePtr;
        }
        else
        {
            this->tail.nextPtr = nodePtr;
            this->tail = nodePtr;
        }
    }
  
    // Function to delete the 
    // node at given position 
    bool removeNode(T* nodePtr)
    {
        T* tempPtr;
        bool nodeFound = false;

        if((this->head == nodePtr) && (this->tail == nodePtr))
        {
            this->head = NULL;
            this->tail = NULL;
            nodeFound = true;
        }
        else if(this->head == nodePtr)
        {
            this->head = nodePtr.nextPtr;
            nodeFound = true;
        }
        else
        {
            for(tempPtr = this->head; tempPtr.nextPtr != NULL; tempPtr = tempPtr.nextPtr)
            {
                if(tempPtr.nextPtr == nodePtr)
                {
                    tempPtr.nextPtr = nodePtr.nextPtr;
                    nodeFound = true;
                }
            }
        }

        if(nodeFound == true)
        {
            delete nodePtr;
        }

        return nodeFound;
    }
};

/*********************Global variables*********************/
extern Linkedlist<sprinklerCfgNode> CFGM_sprinklerCfg;
extern Linkedlist<scheduleCfgNode> CFGM_scheduleCfg;

#endif /* _CFGM_HPP_ */