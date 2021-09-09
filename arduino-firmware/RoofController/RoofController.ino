/*******************************************************************************
 * OBSERVATORY ROLL-OFF-ROOF FIRMWARE
 * 
 * Firmware for a Arduino controller Roll-Off-Roof observatory. A simple 
 * state-machine based on StandardFirmata, comminicates with indiserver indi_rollroof driver.
 * NOTE: Firmata does not function well over USB3. Always use a USB2 port!
 * 
 * Sept 2015 Derek OKeeffe
 * Sept 2019 Mikhail Topchilo (miksoft.pro)
*******************************************************************************/
#include <Wire.h>
#include <Firmata.h>

/**
 * ======= CONFIGURATION =======
 */
// If the variable is not commented out, debug mode is activated, messages are sent to the serial port
// #define DEBUG

const int pinRelayOpen        = 2; // RELAY: Open roof
const int pinRelayClose       = 3; // RELAY: Close roof
const int pinLimitSwitchClose = 4; // SWITCH: Closed roof limit switch 
const int pinLimitSwitchOpen  = 5; // SWITCH: Opened roof limit switch 
const int parkTelescopeDEC    = 6; // SWITCH: Telescope parking position on the axis DEC
const int parkTelescopeRA     = 7; // SWITCH: Telescope parking position on the axis RA
const int ledPin              = 13; // INDICATOR: Pin for connecting a visual indicator of the roof position (for DEBUG)

long ledToggleTime = 0;
bool ledState;
int openState, closeState, parkStateRa, parkStateDec;

/**
 * ======= SETUP =======
 */
void setup()
{
  Firmata.setFirmwareVersion(FIRMATA_MAJOR_VERSION, FIRMATA_MINOR_VERSION);
  Firmata.attach(STRING_DATA, stringCallback);
  Firmata.begin(57600);
  pinMode(pinRelayOpen, OUTPUT);
  pinMode(pinRelayClose, OUTPUT);
  
  pinMode(pinLimitSwitchOpen, INPUT_PULLUP); // INPUT
  pinMode(pinLimitSwitchClose, INPUT_PULLUP); // INPUT

  pinMode(parkTelescopeRA, INPUT_PULLUP);
  pinMode(parkTelescopeDEC, INPUT_PULLUP);
  
  pinMode(ledPin, OUTPUT);

  // By default, the relays should be turned off, so we give them a high signal 
  digitalWrite(pinRelayOpen, HIGH);
  digitalWrite(pinRelayClose, HIGH);
}

/**
 * ======= LOOP =======
 */
void loop()
{
  while (Firmata.available()) {
    Firmata.processInput();
  }

  #ifdef DEBUG
    debugLEDs();
  #endif
}

/**
 * ======= FIRMATA HANDLER =======
 */
void stringCallback(char *myString) {
  openState  = digitalRead(pinLimitSwitchOpen);
  closeState = digitalRead(pinLimitSwitchClose);
  parkStateRa  = digitalRead(parkTelescopeRA);
  parkStateDec = digitalRead(parkTelescopeDEC);
  
  String commandString = String(myString);

  // CMD: Open roof
  if (commandString.equals("OPEN") && (parkStateRa == LOW && parkStateDec == LOW))
    actionRoofOpen();

  // CMD: Close roof
  else if (commandString.equals("CLOSE") && (parkStateRa == LOW && parkStateDec == LOW))
    actionRoofClose();

  // CMD: Abort command
  else if (commandString.equals("ABORT")) {
    return ;
  }

  // CMD: Query status
  /**
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
  else if (commandString.equals("QUERY")) {
    int stateRoof;
    int statePark;

    if (openState == LOW && closeState == HIGH)
      stateRoof = 10; // Open roof
    
    else if (openState == HIGH && closeState == LOW)
      stateRoof = 20; // Close roof

    else
      stateRoof = 30; // Unknow


    if (parkStateDec == LOW && parkStateRa == LOW)
      statePark = 0; // PARK
      
    else if (parkStateDec == HIGH && parkStateRa == HIGH)
      statePark = 1; // NO park DEC, RA 

    else if (parkStateDec == HIGH && parkStateRa == LOW)
      statePark = 2; // NO park DEC

    else if (parkStateDec == LOW && parkStateRa == HIGH)
      statePark = 3; // NO park RA

      char cstr[8];

    return Firmata.sendString( itoa(stateRoof + statePark, cstr, 10) );
  }
}

/**
 * Close roof
 */
void actionRoofClose() {
  unsigned long activateTime = millis();

  digitalWrite(pinRelayOpen, LOW);
  while((millis() - activateTime) < 800) {}
  digitalWrite(pinRelayOpen, HIGH);
}

/**
 * Open roof
 */
void actionRoofOpen() {
  unsigned long activateTime = millis();
  
  digitalWrite(pinRelayClose, LOW);
  while((millis() - activateTime) < 800) {}
  digitalWrite(pinRelayClose, HIGH);
}

/** 
 * LED is used to provide visual clues to the state of the roof controller.
 * Really not necessary, but handy for debugging wiring and mechanical problem.
 */
void debugLEDs() {
  openState  = digitalRead(pinLimitSwitchOpen);
  closeState = digitalRead(pinLimitSwitchClose);
  
  // Opened roof
  if (closeState == HIGH && openState == LOW)
    toggleLed(200);

  // Closed roof
  else if (closeState == LOW && openState == HIGH)
    toggleLed(1000);

  // Where is roof, wtf?
  else
    digitalWrite(ledPin, LOW);
}

void toggleLed(int duration) {
  if (millis() - ledToggleTime > duration) {
    ledToggleTime = millis();
    
    if (ledState == false ) {
      ledState = true;
      digitalWrite(ledPin, HIGH);
    } else {
      ledState = false;
      digitalWrite(ledPin, LOW);
    }
  }
}
