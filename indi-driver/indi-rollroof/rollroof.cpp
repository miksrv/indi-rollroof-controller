/*******************************************************************************
 * OBSERVATORY ROLL-OFF-ROOF DRIVER
 * Controls an arduino using firmata to switch on/off relays.
 * NOTE: Firmata does not function well over USB3. Always use a USB2 port!
 * 
 * Sept 2015 Derek OKeeffe
 * Sept 2019 Mikhail Topchilo (miksoft.pro)
*******************************************************************************/
#include "rollroof.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#include <memory>

#include <indicom.h>
#include "connectionplugins/connectionserial.h"

std::unique_ptr<RollRoof> rollOff(new RollRoof());

// This is the max ontime (sec) for the motors. Safety cut out.
#define MAX_ROLLOFF_DURATION 19 

void ISPoll(void *p);

void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;

    if (rollOff.get() == 0)
        rollOff.reset(new RollRoof());

}

void ISGetProperties(const char *dev)
{
    ISInit();
    rollOff->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    ISInit();
    rollOff->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
    ISInit();
    rollOff->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    ISInit();
    rollOff->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISSnoopDevice (XMLEle *root)
{
    ISInit();
    rollOff->ISSnoopDevice(root);
}

RollRoof::RollRoof()
{
  fullOpenLimitSwitch   = ISS_OFF;
  fullClosedLimitSwitch = ISS_OFF;
  bTelescopeParkingDEC  = false;
  bTelescopeParkingRA   = false;

  MotionRequest = 0;

  SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_PARK);
}

/**
 * Init all properties
 */
bool RollRoof::initProperties()
{
    DEBUG(INDI::Logger::DBG_SESSION, "Init props");

    INDI::Dome::initProperties();

    SetParkDataType(PARK_NONE);
    addAuxControls();

    IUFillText(&CurrentStateT[0], "ROOF", "Position status", NULL);
    IUFillTextVector(&CurrentStateTP, CurrentStateT, 1, getDeviceName(), "ROOF", "Roll-off-roof", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    IUFillText(&CurrentParkT[0], "TELESCOPE", "Park status", NULL);
    IUFillTextVector(&CurrentParkTP, CurrentParkT, 1, getDeviceName(), "TELESCOPE", "Telescope", MAIN_CONTROL_TAB,IP_RO, 60, IPS_IDLE);

    return true;
}

bool RollRoof::ISSnoopDevice (XMLEle *root)
{
	return INDI::Dome::ISSnoopDevice(root);
}

bool RollRoof::Connect()
{
    sf = new Firmata(serialConnection->port());
    if (sf->portOpen) {
        sf->OnIdle();

		if (strstr(sf->firmata_name, "RoofController")) {
			DEBUG(INDI::Logger::DBG_SESSION, "ARDUINO BOARD CONNECTED");
			DEBUGF(INDI::Logger::DBG_SESSION, "FIRMATA VERSION: %s",sf->firmata_name);
			return true;
		} else {
		    DEBUG(INDI::Logger::DBG_SESSION, "ARDUINO BOARD INCOMPATABLE FIRMWARE");
		    DEBUGF(INDI::Logger::DBG_SESSION, "FIRMATA VERSION: %s",sf->firmata_name);
		    return false;
		}

    } else {
        DEBUG(INDI::Logger::DBG_SESSION, "ARDUINO BOARD FAIL TO CONNECT");
        delete sf;
        return false;
    }
}

RollRoof::~RollRoof()
{

}

const char * RollRoof::getDefaultName()
{
    return (char *)"Roll Roof";
}

bool RollRoof::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}

/**
 * Update properties from Arduino controller
 */
bool RollRoof::updateProperties()
{
    DEBUG(INDI::Logger::DBG_SESSION, "Updating controller props ...");
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        fullOpenLimitSwitch   = ISS_OFF;
        fullClosedLimitSwitch = ISS_OFF;
        bTelescopeParkingDEC  = false;
        bTelescopeParkingRA   = false;

        getControllerState();

        defineText(&CurrentStateTP);
        defineText(&CurrentParkTP);
    } 

    else {
	   deleteProperty(CurrentStateTP.name);
       deleteProperty(CurrentParkTP.name);
    }

    return true;
}

/**
* Disconnect from the Arduino
*/
bool RollRoof::Disconnect()
{
    sf->closePort();
    delete sf;

    MotionRequest =- 1;

    DEBUG(INDI::Logger::DBG_SESSION, "ARDUINO BOARD DISCONNECTED");
    IDSetSwitch (getSwitch("CONNECTION"),"DISCONNECTED\n");

    return true;
}

/**
 * TimerHit gets called by the indi client every 1 sec when the roof is moving.
 */
void RollRoof::TimerHit()
{

    DEBUG(INDI::Logger::DBG_DEBUG, "Timer hit");
    
    if (isConnected() == false) return;  //  No need to reset timer if we are not connected anymore
    if (DomeMotionSP.s != IPS_BUSY) return;

    // (ABORT) The safety timer has expired called MAX_ROLLOFF_DURATION
    if (MotionRequest < 0) {
        DEBUG(INDI::Logger::DBG_WARNING, "Roof motion is stopped, the safety timer has expired");
        setDomeState(DOME_IDLE);

        string stateString = "ABORTED";
        char roofStatus[32];
        strcpy(roofStatus, stateString.c_str());

        IUSaveText(&CurrentStateT[0], roofStatus);
        IDSetText(&CurrentStateTP, NULL);

        SetTimer(500);

        return;
    }

    // Roll off is OPENING
    if (DomeMotionS[DOME_CW].s == ISS_ON) {
        string stateString = "MOVE: OPENING...";

        if (getFullOpenedLimitSwitch()) {
            setDomeState(DOME_UNPARKED);
            SetParked(false);
            IUResetSwitch(&ParkSP);

            ParkS[1].s = ISS_ON;
            ParkSP.s = IPS_OK;

            updateProperties();

            return;
        }

        char status[32];

        strcpy(status, stateString.c_str());

        IUSaveText(&CurrentStateT[0], status);
        IDSetText(&CurrentStateTP, NULL);

        if (CalcTimeLeft(MotionStart) <= 0) {
            DEBUG(INDI::Logger::DBG_WARNING, "Exceeded max motor run duration. Aborting.");
            Abort();
        }
    }

    // Roll Off is CLOSING
    else if (DomeMotionS[DOME_CCW].s == ISS_ON) {
        string stateString = "MOVE: CLOSING...";

        if (getFullClosedLimitSwitch()) {
            setDomeState(DOME_PARKED);
            SetParked(true);

            updateProperties();

            return;
        }

        char status[32];

        strcpy(status, stateString.c_str());

        IUSaveText(&CurrentStateT[0], status);
        IDSetText(&CurrentStateTP, NULL);

        if (CalcTimeLeft(MotionStart) <= 0) {
            DEBUG(INDI::Logger::DBG_WARNING, "Exceeded max motor run duration. Aborting.");
            Abort();
        }
    }

    SetTimer(500);
}

bool RollRoof::saveConfigItems(FILE *fp)
{
    return INDI::Dome::saveConfigItems(fp);
}

/**
 * Move the Roof. Send the command string over Firmata to the Arduino.
 */
IPState RollRoof::Move(DomeDirection dir, DomeMotionCommand operation)
{
    if (operation == MOTION_START)
    {
        updateProperties();

        sleep(1);

        if ( ! bTelescopeParkingDEC || ! bTelescopeParkingRA) {
            DEBUG(INDI::Logger::DBG_WARNING, "The telescope is NOT PARKED, the roof cannot be moved!");
            return IPS_ALERT;
        }

        // If there is a signal to OPEN the roof, but it is already fully open 
        else if (dir == DOME_CW && fullOpenLimitSwitch == ISS_ON) {
            DEBUG(INDI::Logger::DBG_WARNING, "Roof is already fully OPENED");
            return IPS_ALERT;
        }

        // If there is a signal to CLOSE the roof, but it is already closed 
        else if (dir == DOME_CCW && fullClosedLimitSwitch == ISS_ON) {
            DEBUG(INDI::Logger::DBG_WARNING, "Roof is already fully CLOSED");
            return IPS_ALERT;
        }

        // If the telescope is not programmatically parked 
        else if (dir == DOME_CCW && INDI::Dome::isLocked()) {
            DEBUG(INDI::Logger::DBG_WARNING, "Cannot close dome when mount is locking. See: Telescope parkng policy, in options tab");
            return IPS_ALERT;
        }

        // Roof open signal 
        else if (dir == DOME_CW) {
            DEBUG(INDI::Logger::DBG_SESSION, "Sending command OPEN ...");
            sleep(1);
            sf->sendStringData((char *)"OPEN");
        }

        // Roof close signal 
        else if (dir == DOME_CCW) {
            DEBUG(INDI::Logger::DBG_SESSION, "Sending command CLOSE ...");
            sleep(1);
            sf->sendStringData((char *)"CLOSE");
        }

        MotionRequest = MAX_ROLLOFF_DURATION;
        gettimeofday(&MotionStart,NULL);
        SetTimer(500);

        // DEBUG(INDI::Logger::DBG_SESSION, "Return IPS_BUSY");

        return IPS_BUSY;
    }
    else
    {
        DEBUG(INDI::Logger::DBG_SESSION, "WTF WTF ");
        return (Dome::Abort() ? IPS_OK : IPS_ALERT);

    }

    DEBUG(INDI::Logger::DBG_SESSION, "Return IPS_ALERT");
    return IPS_ALERT;
}

/**
 * Park the Roof = CLOSE
 */
IPState RollRoof::Park()
{
    IPState rc = INDI::Dome::Move(DOME_CCW, MOTION_START);
    if (rc==IPS_BUSY)
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Roll off is parking...");
        return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

/**
 * Unpark the Roof = OPEN
 */
IPState RollRoof::UnPark()
{
    IPState rc = INDI::Dome::Move(DOME_CW, MOTION_START);
    if (rc==IPS_BUSY) {
           DEBUG(INDI::Logger::DBG_SESSION, "Roll off is unparking...");
           return IPS_BUSY;
    }
    else
      return IPS_ALERT;
}

/**
 * Abort command and update current state from Arduino.
 */
bool RollRoof::Abort()
{
    DEBUG(INDI::Logger::DBG_SESSION, "Sending command ABORT");
    sf->sendStringData((char *)"ABORT");
    sf->OnIdle();
    MotionRequest =- 1;

    updateProperties();

    return true;
}

/**
 * Timer for safety countdown.
 */
float RollRoof::CalcTimeLeft(timeval start)
{
    double timesince;
    double timeleft;
    struct timeval now;

    gettimeofday(&now,NULL);

    timesince = (double)(now.tv_sec * 1000.0 + now.tv_usec / 1000) - (double)(start.tv_sec * 1000.0 + start.tv_usec / 1000);
    timesince = timesince / 1000;
    timeleft  = MotionRequest - timesince;

    return timeleft;
}

/**
 * Get state from Arduino controller.
 * 
 * 10 - Roof closed, telescope parked
 * 20 - Roof open, telescope parked
 *
 * 11 - Roof closed, no parking (DEC, RA)
 * 12 - Roof closed, no parking (DEC)
 * 13 - Roof closed, no parking (RA)
 *
 * 21 - Roof open, no parking (DEC, RA)
 * 22 - Roof open, no parking (DEC)
 * 23 - Roof open, no parking (RA)
 */
bool RollRoof::getControllerState()
{
    string roofStateString = "UNKNOWN";
    string parkStateString = "UNKNOWN";
    
    // DEBUG(INDI::Logger::DBG_SESSION, "QUERY: Get controller state");

    sf->sendStringData((char*)"QUERY");
    sf->OnIdle();

    string stateString = sf->string_buffer;

    // DEBUGF(INDI::Logger::DBG_SESSION, "QUERY: Response = %s", stateString.c_str());

    char roofStatusInt = stateString[0];
    char parkStatusInt = stateString[1];

    if (roofStatusInt == '1') {
        DEBUG(INDI::Logger::DBG_SESSION, "Roof position state: Open");

        setDomeState(DOME_UNPARKED);
        fullOpenLimitSwitch = ISS_ON;
        roofStateString     = "OPEN";
    }

    else if (roofStatusInt == '2') {
        DEBUG(INDI::Logger::DBG_SESSION, "Roof position state: Close");

        setDomeState(DOME_PARKED);
        fullClosedLimitSwitch = ISS_ON;
        roofStateString       = "CLOSE";
    }

    else if (roofStatusInt == '3') {
        setDomeState(DOME_IDLE);
        DEBUG(INDI::Logger::DBG_SESSION, "Roof position state: UNKNOWN");
    }


    if (parkStatusInt == '0') {
        DEBUG(INDI::Logger::DBG_SESSION, "Telescope parkng state: Parked");

        bTelescopeParkingRA  = true;
        bTelescopeParkingDEC = true;
        parkStateString      = "PARKED";
    }

    else if (parkStatusInt == '1') {
        DEBUG(INDI::Logger::DBG_SESSION, "Telescope parkng state: No park DEC, RA");
        parkStateString = "NO PARKED (DEC, RA)";
    }

    else if (parkStatusInt == '2') {
        DEBUG(INDI::Logger::DBG_SESSION, "Telescope parkng state: No park DEC");

        bTelescopeParkingRA = true;
        parkStateString     = "NO PARKED (DEC)";
    }

    else if (parkStatusInt == '3') {
        DEBUG(INDI::Logger::DBG_SESSION, "Telescope parkng state: No park RA");

        bTelescopeParkingDEC = true;
        parkStateString      = "NO PARKED (RA)";
    }

    char roofStatus[32];
    char parkStatus[32];

    strcpy(roofStatus, roofStateString.c_str());
    strcpy(parkStatus, parkStateString.c_str());

    IUSaveText(&CurrentStateT[0], roofStatus);
    IDSetText(&CurrentStateTP, NULL);

    IUSaveText(&CurrentParkT[0], parkStatus);
    IDSetText(&CurrentParkTP, NULL);

    sleep(1);

    return true;
}

/**
 * Get the state of the full open limit switch. This function will also switch off the motors as a safety override.
 */
bool RollRoof::getFullOpenedLimitSwitch()
{
    sf->sendStringData((char*)"QUERY");
    sf->OnIdle();

    string stateString = sf->string_buffer;

    // DEBUGF(INDI::Logger::DBG_SESSION, "QUERY: Roof moving, response = %s", stateString.c_str());

    char roofStatusInt = stateString[0];

    if (roofStatusInt == '1') {
        fullOpenLimitSwitch = ISS_ON;

        return true;
    }

    return false;
}

/**
 * Get the state of the full closed limit switch. This function will also switch off the motors as a safety override.
 */
bool RollRoof::getFullClosedLimitSwitch()
{
    sf->sendStringData((char*)"QUERY");
    sf->OnIdle();

    string stateString = sf->string_buffer;

    // DEBUGF(INDI::Logger::DBG_SESSION, "QUERY: Roof moving, response = %s", stateString.c_str());

    char roofStatusInt = stateString[0];

    if (roofStatusInt == '2') {
        fullClosedLimitSwitch = ISS_ON;

        return true;
    }

    return false;
}
