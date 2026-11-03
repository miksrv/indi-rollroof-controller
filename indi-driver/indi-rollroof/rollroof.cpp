/*******************************************************************************
 * OBSERVATORY ROLL-OFF-ROOF DRIVER
 * Controls an arduino using firmata to switch on/off relays.
 * NOTE: Firmata does not function well over USB3. Always use a USB2 port!
 *
 * Sept 2015 Derek OKeeffe
 * Sept 2019 Mikhail Topchilo (miksoft.pro)
 * Oct 2026 Ported to INDI 2.x
*******************************************************************************/
#include "rollroof.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#include <memory>
#include <string>

#include <indicom.h>
#include "connectionplugins/connectionserial.h"

// INDI 2.x dispatches ISGetProperties/ISNew* to every DefaultDevice instance itself
static std::unique_ptr<RollRoof> rollOff(new RollRoof());

// This is the max ontime (sec) for the motors. Safety cut out.
#define MAX_ROLLOFF_DURATION 19

// How often the controller is queried while a travel is in progress (ms)
#define MOTION_POLL_MS 500

// Default interval of the status query while the roof is idle (ms, the standard Polling period)
#define DEFAULT_STATUS_POLL_MS 5000

// Shortest idle poll, and the poll while the controller does not answer (ms)
#define MIN_STATUS_POLL_MS     1000
#define OFFLINE_STATUS_POLL_MS 30000

// A roof change this long after the driver's own OPEN/CLOSE is not "moved by other means" (s)
#define OWN_TRAVEL_GRACE 60

RollRoof::RollRoof()
{
  fullOpenLimitSwitch   = ISS_OFF;
  fullClosedLimitSwitch = ISS_OFF;
  bTelescopeParkingDEC  = false;
  bTelescopeParkingRA   = false;

  MotionRequest = 0;

  // Reported in DRIVER_INFO; the numbers come from CMakeLists.txt (INDI keeps major.minor only)
  setVersion(ROLLROOF_VERSION_MAJOR, ROLLROOF_VERSION_MINOR);

  SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_PARK);
}

/**
 * Init all properties
 */
bool RollRoof::initProperties()
{
    INDI::Dome::initProperties();

    SetParkDataType(PARK_NONE);

    // Options -> Polling: how often the roof and the reed switches are checked while idle
    setDefaultPollingPeriod(DEFAULT_STATUS_POLL_MS);
    addAuxControls();

    IUFillText(&CurrentStateT[0], "ROOF", "Position status", NULL);
    IUFillTextVector(&CurrentStateTP, CurrentStateT, 1, getDeviceName(), "ROOF", "Roll-off-roof", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    IUFillText(&CurrentParkT[0], "TELESCOPE", "Park status", NULL);
    IUFillTextVector(&CurrentParkTP, CurrentParkT, 1, getDeviceName(), "TELESCOPE", "Telescope", MAIN_CONTROL_TAB,IP_RO, 60, IPS_IDLE);

    return true;
}

bool RollRoof::ISSnoopDevice (XMLEle *root)
{
    auto activeTelescope = ActiveDeviceTP.findWidgetByName("ACTIVE_TELESCOPE");
    const char *device   = findXMLAttValu(root, "device");
    const char *propName = findXMLAttValu(root, "name");

    if (activeTelescope != nullptr && !strcmp(device, activeTelescope->getText()))
    {
        // Mount driver went away (whole device or its park property deleted) -> state unknown
        if (!strcmp(tagXMLEle(root), "delProperty") && (propName[0] == '\0' || !strcmp(propName, "TELESCOPE_PARK")))
        {
            if (mountParkKnown)
                LOG_WARN("Mount park state is UNKNOWN (mount driver disconnected). Roof motion is blocked.");

            mountParkKnown = false;
            mountParked    = false;
        }

        else if (!strcmp(propName, "TELESCOPE_PARK"))
        {
            bool parkFound = false;
            bool parkOn    = false;

            for (XMLEle *ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            {
                if (!strcmp(findXMLAttValu(ep, "name"), "PARK"))
                {
                    parkFound = true;
                    parkOn    = strstr(pcdataXMLEle(ep), "On") != nullptr;
                }
            }

            if (parkFound)
            {
                // Only a settled (Ok) PARK=On counts; Busy (parking/unparking) and Alert do not
                bool parked = parkOn && !strcmp(findXMLAttValu(root, "state"), "Ok");

                if (!mountParkKnown || parked != mountParked)
                    LOGF_INFO("Mount %s software park state: %s", device, parked ? "PARKED" : "NOT PARKED");

                mountParkKnown = true;
                mountParked    = parked;
            }
        }
    }

	return INDI::Dome::ISSnoopDevice(root);
}

void RollRoof::ActiveDevicesUpdated()
{
    // A different mount was selected, forget everything we knew about the previous one
    mountParkKnown = false;
    mountParked    = false;
}

bool RollRoof::isMountSoftwareParked()
{
    return mountParkKnown && mountParked;
}

bool RollRoof::Connect()
{
    sf = new Firmata(serialConnection->port());
    if (sf->portOpen) {
        sf->OnIdle();

		if (strstr(sf->firmata_name, "RoofController")) {
			LOG_INFO("ARDUINO BOARD CONNECTED");
			LOGF_INFO("FIRMATA VERSION: %s", sf->firmata_name);
			return true;
		} else {
		    LOG_ERROR("ARDUINO BOARD INCOMPATABLE FIRMWARE");
		    LOGF_ERROR("FIRMATA VERSION: %s", sf->firmata_name);
		    sf->closePort();
		    delete sf;
		    sf = nullptr;
		    return false;
		}

    } else {
        LOGF_ERROR("ARDUINO BOARD FAIL TO CONNECT (%s)", serialConnection->port());
        delete sf;
        sf = nullptr;
        return false;
    }
}

RollRoof::~RollRoof()
{

}

const char * RollRoof::getDefaultName()
{
    return "Roll Roof";
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
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        // Saved park state first, so the first query only changes it if the roof really moved
        InitPark();

        // Publish the full state on (re)connect, then keep it fresh while idle
        lastRoofState       = 0;
        lastParkState       = 0;
        controllerAnswering = true;
        getControllerState();

        defineProperty(&CurrentStateTP);
        defineProperty(&CurrentParkTP);

        armTimer(statusPollMs());
    }

    else {
        stopTimer();

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
    stopTimer();

    if (sf != nullptr) {
        sf->closePort();
        delete sf;
        sf = nullptr;
    }

    MotionRequest = -1;

    LOG_INFO("ARDUINO BOARD DISCONNECTED");

    return true;
}

void RollRoof::armTimer(uint32_t ms)
{
    if (timerID >= 0)
        RemoveTimer(timerID);

    timerID = SetTimer(ms);
}

void RollRoof::stopTimer()
{
    if (timerID >= 0)
        RemoveTimer(timerID);

    timerID = -1;
}

uint32_t RollRoof::statusPollMs()
{
    uint32_t ms = getCurrentPollingPeriod();

    if (ms < MIN_STATUS_POLL_MS)
        ms = MIN_STATUS_POLL_MS;

    if (!controllerAnswering && ms < OFFLINE_STATUS_POLL_MS)
        ms = OFFLINE_STATUS_POLL_MS;

    return ms;
}

void RollRoof::showUnknownState()
{
    if (lastRoofState != '?') {
        IUSaveText(&CurrentStateT[0], "UNKNOWN");
        IDSetText(&CurrentStateTP, NULL);
        IUSaveText(&CurrentParkT[0], "UNKNOWN");
        IDSetText(&CurrentParkTP, NULL);
    }

    lastRoofState = '?';
    lastParkState = '?';
}

void RollRoof::showRoofText(const char *text)
{
    IUSaveText(&CurrentStateT[0], text);
    IDSetText(&CurrentStateTP, NULL);

    // The next query has to publish the real state again, even if it did not change
    lastRoofState = 0;
}

/**
 * While the roof is idle: query the controller every STATUS_POLL seconds, so a roof moved by other
 * means and the park reed switches are always shown as they are (only changes are published).
 * While a travel is in progress: watch the target limit switch every MOTION_POLL_MS.
 */
void RollRoof::TimerHit()
{
    timerID = -1;

    if (isConnected() == false) return;  //  No need to reset timer if we are not connected anymore

    if (DomeMotionSP.getState() != IPS_BUSY) {
        getControllerState();
        armTimer(statusPollMs());

        return;
    }

    LOG_DEBUG("Timer hit");

    // (ABORT) The safety timer has expired called MAX_ROLLOFF_DURATION
    if (MotionRequest < 0) {
        LOG_WARN("Roof motion is stopped, the safety timer has expired");
        setDomeState(DOME_IDLE);

        showRoofText("ABORTED");
        armTimer(statusPollMs());

        return;
    }

    // Roll off is OPENING
    if (DomeMotionSP[DOME_CW].getState() == ISS_ON) {
        if (getFullOpenedLimitSwitch()) {
            SetParked(false);
            getControllerState();
            armTimer(statusPollMs());

            return;
        }

        showRoofText("MOVE: OPENING...");

        if (CalcTimeLeft(MotionStart) <= 0) {
            LOG_WARN("Exceeded max motor run duration. Aborting.");
            Abort();
        }
    }

    // Roll Off is CLOSING
    else if (DomeMotionSP[DOME_CCW].getState() == ISS_ON) {
        if (getFullClosedLimitSwitch()) {
            SetParked(true);
            getControllerState();
            armTimer(statusPollMs());

            return;
        }

        showRoofText("MOVE: CLOSING...");

        if (CalcTimeLeft(MotionStart) <= 0) {
            LOG_WARN("Exceeded max motor run duration. Aborting.");
            Abort();
        }
    }

    armTimer(DomeMotionSP.getState() == IPS_BUSY ? MOTION_POLL_MS : statusPollMs());
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
        // Safety interlock: the roof moves ONLY when the mount is parked both in
        // software (mount driver reports PARK) and in hardware (both reed switches)
        if ( ! isMountSoftwareParked()) {
            LOG_ERROR("The telescope is NOT PARKED in software (or its park state is unknown), the roof cannot be moved!");
            return IPS_ALERT;
        }

        if ( ! getControllerState()) {
            LOG_ERROR("Roof controller state is unknown, the roof cannot be moved!");
            return IPS_ALERT;
        }

        if ( ! bTelescopeParkingDEC || ! bTelescopeParkingRA) {
            LOG_ERROR("The telescope is NOT PARKED by the reed switches, the roof cannot be moved!");
            return IPS_ALERT;
        }

        // If there is a signal to OPEN the roof, but it is already fully open
        else if (dir == DOME_CW && fullOpenLimitSwitch == ISS_ON) {
            LOG_WARN("Roof is already fully OPENED");
            return IPS_ALERT;
        }

        // If there is a signal to CLOSE the roof, but it is already closed
        else if (dir == DOME_CCW && fullClosedLimitSwitch == ISS_ON) {
            LOG_WARN("Roof is already fully CLOSED");
            return IPS_ALERT;
        }

        // If the telescope is not programmatically parked
        else if (dir == DOME_CCW && INDI::Dome::isLocked()) {
            LOG_WARN("Cannot close dome when mount is locking. See: Telescope parkng policy, in options tab");
            return IPS_ALERT;
        }

        // Roof open signal
        else if (dir == DOME_CW) {
            LOG_INFO("Sending command OPEN ...");
            sf->sendStringData((char *)"OPEN");
        }

        // Roof close signal
        else if (dir == DOME_CCW) {
            LOG_INFO("Sending command CLOSE ...");
            sf->sendStringData((char *)"CLOSE");
        }

        gettimeofday(&lastCommand, NULL);

        MotionRequest = MAX_ROLLOFF_DURATION;
        gettimeofday(&MotionStart,NULL);
        armTimer(MOTION_POLL_MS);

        return IPS_BUSY;
    }

    return (Abort() ? IPS_OK : IPS_ALERT);
}

/**
 * Park the Roof = CLOSE
 */
IPState RollRoof::Park()
{
    IPState rc = INDI::Dome::Move(DOME_CCW, MOTION_START);
    if (rc==IPS_BUSY)
    {
        LOG_INFO("Roll off is parking...");
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
           LOG_INFO("Roll off is unparking...");
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
    if (sf == nullptr)
        return false;

    LOG_INFO("Sending command ABORT");
    sf->sendStringData((char *)"ABORT");
    sf->OnIdle();
    MotionRequest = -1;

    getControllerState();

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
 * Send QUERY and wait (up to ~1 sec) for the controller to answer.
 * The previous answer is cleared first, so a stale state is never reused.
 */
bool RollRoof::queryController()
{
    if (sf == nullptr)
        return false;

    sf->string_buffer[0] = '\0';
    sf->sendStringData((char*)"QUERY");

    // OnIdle() waits up to 10 ms for serial data
    for (int i = 0; i < 100 && sf->string_buffer[0] == '\0'; i++) {
        if (sf->OnIdle() < 0)
            break;
    }

    if (sf->string_buffer[0] == '\0') {
        // Once per outage, not on every status poll
        if (controllerAnswering)
            LOG_WARN("No response from the roof controller");

        controllerAnswering = false;
        return false;
    }

    if (!controllerAnswering)
        LOG_INFO("The roof controller answers again");

    controllerAnswering = true;
    return true;
}

/**
 * Get state from Arduino controller.
 *
 * The answer is two digits: roof state + park state.
 * Roof: 1 - open (open limit switch pressed), 2 - closed (closed limit switch pressed),
 *       3 - unknown (both or neither pressed, e.g. while moving)
 * Park: 0 - telescope parked (both reed switches closed), 1 - not parked (DEC, RA),
 *       2 - not parked (DEC), 3 - not parked (RA)
 *
 * E.g. 20 - roof closed, telescope parked; 13 - roof open, RA not parked.
 *
 * Called before every motion, while a travel is watched and every STATUS_POLL seconds while idle:
 * the limit switch flags are always fresh, but the texts, the dome park state and the log only
 * change when the answer changes.
 */
bool RollRoof::getControllerState()
{
    // Every query starts from scratch, otherwise a limit switch seen once stays ON forever
    fullOpenLimitSwitch   = ISS_OFF;
    fullClosedLimitSwitch = ISS_OFF;
    bTelescopeParkingDEC  = false;
    bTelescopeParkingRA   = false;

    if ( ! queryController()) {
        showUnknownState();
        return false;
    }

    std::string stateString = sf->string_buffer;

    LOGF_DEBUG("QUERY: Response = %s", stateString.c_str());

    if (stateString.size() < 2) {
        // Logged once, like a missing answer, not on every poll
        if (lastRoofState != '?')
            LOGF_WARN("Unexpected controller response: '%s'", stateString.c_str());

        showUnknownState();
        return false;
    }

    char roofStatusInt = stateString[0];
    char parkStatusInt = stateString[1];

    const char *roofStateString = "UNKNOWN";
    const char *parkStateString = "UNKNOWN";

    if (roofStatusInt == '1') {
        fullOpenLimitSwitch = ISS_ON;
        roofStateString     = "OPEN";
    }

    else if (roofStatusInt == '2') {
        fullClosedLimitSwitch = ISS_ON;
        roofStateString       = "CLOSED";
    }

    if (parkStatusInt == '0') {
        bTelescopeParkingRA  = true;
        bTelescopeParkingDEC = true;
        parkStateString      = "PARKED";
    }

    else if (parkStatusInt == '1') {
        parkStateString = "NO PARKED (DEC, RA)";
    }

    else if (parkStatusInt == '2') {
        bTelescopeParkingRA = true;
        parkStateString     = "NO PARKED (DEC)";
    }

    else if (parkStatusInt == '3') {
        bTelescopeParkingDEC = true;
        parkStateString      = "NO PARKED (RA)";
    }

    bool changed  = roofStatusInt != lastRoofState;
    bool watching = DomeMotionSP.getState() == IPS_BUSY;

    if (changed) {
        bool known = lastRoofState == '1' || lastRoofState == '2' || lastRoofState == '3';

        // A travel this driver started may still end after an abort or the safety timeout
        struct timeval now;
        gettimeofday(&now, NULL);
        bool ownTravel = lastCommand.tv_sec != 0 && now.tv_sec - lastCommand.tv_sec < OWN_TRAVEL_GRACE;

        // The roof changed while the driver did not move it: the gate controller's own remote,
        // a power relay or someone on site
        if (known && !watching && !ownTravel)
            LOGF_WARN("Roof position changed without a command from this driver: %s", roofStateString);
        else
            LOGF_INFO("Roof position state: %s", roofStateString);

        IUSaveText(&CurrentStateT[0], roofStateString);
        IDSetText(&CurrentStateTP, NULL);
    }

    // The dome state follows the limit switches whenever they changed, and always at the end of
    // a watched travel or an abort (that is also what ends DOME_MOTION Busy)
    if (changed || watching) {
        if (roofStatusInt == '1' || roofStatusInt == '2') {
            bool closed = roofStatusInt == '2';

            // Keep DOME_PARK (and the saved park state) in line with the limit switches
            if (isParked() != closed)
                SetParked(closed);
            else if (watching) {
                // End DOME_MOTION Busy and re-assert the park state (what SetParked does, without
                // rewriting the park data file)
                setDomeState(DOME_IDLE);
                setDomeState(closed ? DOME_PARKED : DOME_UNPARKED);
            }
        }

        // Neither limit switch (stopped part-way, or still travelling after an abort): not
        // parked, so a shutdown really closes it; DOME_PARK goes Idle with neither switch on,
        // which keeps a mount with Dome locks locked (it only follows a settled PARK/UNPARK)
        else {
            setDomeState(DOME_IDLE);
            setDomeState(DOME_UNKNOWN);
        }
    }

    if (parkStatusInt != lastParkState) {
        LOGF_INFO("Telescope park state (reed switches): %s", parkStateString);

        IUSaveText(&CurrentParkT[0], parkStateString);
        IDSetText(&CurrentParkTP, NULL);
    }

    lastRoofState = roofStatusInt;
    lastParkState = parkStatusInt;

    return true;
}

/**
 * Get the state of the full open limit switch. This function will also switch off the motors as a safety override.
 */
bool RollRoof::getFullOpenedLimitSwitch()
{
    if ( ! queryController())
        return false;

    std::string stateString = sf->string_buffer;

    if (!stateString.empty() && stateString[0] == '1') {
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
    if ( ! queryController())
        return false;

    std::string stateString = sf->string_buffer;

    if (!stateString.empty() && stateString[0] == '2') {
        fullClosedLimitSwitch = ISS_ON;

        return true;
    }

    return false;
}
