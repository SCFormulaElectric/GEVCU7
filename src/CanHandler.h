/*
 * CanHandler.h

 Copyright (c) 2013 Collin Kidder, Michael Neuweiler, Charles Galpin

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Using Tony's CAN library since it is much improved and debugged for the Teensy processor. That presents an issue
as this code was always somewhat hard coded around the SAM3X due_can library. So, the due_can struct must be copied
into this file so that the external libraries still use that format. This code will then translate back and forth
between the flexcan struct and the due_can struct so that the existing device modules all still work the same between
GEVCU6 and GEVCU7.
*/

#ifndef CAN_HANDLER_H_
#define CAN_HANDLER_H_

#include <Arduino.h>
#include "config.h"
#include <FlexCAN_T4.h>
#include "Logger.h"

/*
 * CAN BUS CONFIGURATION
 */
#define CFG_CAN0_SPEED                              500 // specify the speed of the CAN0 bus (EV) in thousands. 
#define CFG_CAN1_SPEED                              500 // specify the speed of the CAN1 bus (Car) in thousands
#define CFG_CAN2_SPEED                              500 // speed of third CAN channel. 
#define CFG_SWCAN_SPEED                             33  //shares with another CAN channel but when active this is the default speed

//CAN message ID ASSIGNMENTS FOR I/0 MANAGEMENT
//should make these configurable.
#define CAN_SWITCH 0x606
#define CAN_OUTPUTS 0x607
#define CAN_ANALOG_INPUTS 0x608
#define CAN_DIGITAL_INPUTS 0x609

enum SDO_COMMAND
{
    SDO_WRITE = 0x20,
    SDO_READ = 0x40,
    SDO_WRITEACK = 0x60,
};

struct SDO_FRAME
{
    uint8_t nodeID;
    SDO_COMMAND cmd;
    uint16_t index;
    uint8_t subIndex;
    uint8_t dataLength;
    uint8_t data[4];
};

enum ISOTP_MODE
{
    SINGLE = 0,
    FIRST = 1,
    CONSEC = 2,
    FLOW = 3
};

class CanObserver
{
public:
    CanObserver();
    virtual void handleCanFrame(const CAN_message_t &frame);
    virtual void handlePDOFrame(const CAN_message_t &frame);
    virtual void handleSDORequest(SDO_FRAME &frame);
    virtual void handleSDOResponse(SDO_FRAME &frame);
    void setCANOpenMode(bool en);
    bool isCANOpen();
    void setNodeID(unsigned int id);
    unsigned int getNodeID();

private:
    bool canOpenMode;
    unsigned int nodeID;
};

class CanHandler
{
public:
    enum CanBusNode {
        CAN_BUS_EV, // CAN0 is intended to be connected to the EV bus (controller, charger, etc.)
        CAN_BUS_CAR, // CAN2 is intended to be connected to the car's high speed bus (the one with the ECU)
        CAN_BUS_CAR2, // CAN1 is an extra bus that is shared with SWCAN. use one or the other. 
        CAN_BUS_SW    //Single wire CAN. Shares CAN hardware with CAN1 so you can only use one or the other
    };

    CanHandler(CanBusNode busNumber);
    void setup();
    uint32_t getBusSpeed();
    void setBusSpeed(uint32_t newSpeed);
    void attach(CanObserver *observer, uint32_t id, uint32_t mask, bool extended);
    void detach(CanObserver *observer, uint32_t id, uint32_t mask);
    void process(const CAN_message_t &msg);
    void prepareOutputFrame(CAN_message_t &frame, uint32_t id);
    void CANIO(const CAN_message_t& frame);
    void sendFrame(const CAN_message_t& frame);
    void sendISOTP(int id, int length, uint8_t *data);

    //canopen support functions
    void sendNodeStart(int id = 0);
    void sendNodePreop(int id = 0);
    void sendNodeReset(int id = 0);
    void sendNodeStop(int id = 0);
    void sendPDOMessage(int, int, unsigned char *);
    void sendSDORequest(SDO_FRAME &frame);
    void sendSDOResponse(SDO_FRAME &frame);
    void sendHeartbeat();
    void setMasterID(int id);

protected:

private:
    struct CanObserverData {
        uint32_t id;    // what id to listen to
        uint32_t mask;  // the CAN frame mask to listen to
        bool extended;  // are extended frames expected
        uint8_t mailbox;    // which mailbox is this observer assigned to
        CanObserver *observer;  // the observer object (e.g. a device)
    };

    CanBusNode canBusNode;  // indicator to which can bus this instance is assigned to
    CanObserverData observerData[CFG_CAN_NUM_OBSERVERS];    // Can observers
    uint32_t busSpeed;

    void logFrame(const CAN_message_t &msg);
    int8_t findFreeObserverData();
    int8_t findFreeMailbox();

    //canopen support functions
    void sendNMTMsg(int, int);
    int masterID; //what is our ID as the master node?      
};

extern CanHandler canHandlerEv;
extern CanHandler canHandlerCar;
extern CanHandler canHandlerCar2;
extern CanHandler canHandlerSingleWire;

#endif /* CAN_HANDLER_H_ */