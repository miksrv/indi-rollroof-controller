#ifndef RollRoof_H
#define RollRoof_H

#include <indidome.h>

#include <math.h>
#include <sys/time.h>

#include "firmata.h"

class RollRoof : public INDI::Dome
{
    public:
        RollRoof();
        virtual ~RollRoof() override;

        virtual bool initProperties() override;
        const char *getDefaultName() override;
        bool updateProperties() override;
        virtual bool ISSnoopDevice (XMLEle *root) override;
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool saveConfigItems(FILE *fp) override;

      protected:

        bool Connect() override;
        bool Disconnect() override;

        void TimerHit() override;

        virtual IPState Move(DomeDirection dir, DomeMotionCommand operation) override;
        virtual IPState Park() override;
        virtual IPState UnPark() override;
        virtual bool Abort() override;

        virtual bool getControllerState();
        virtual bool getFullOpenedLimitSwitch();
        virtual bool getFullClosedLimitSwitch();

        void ActiveDevicesUpdated() override;

    private:

        bool queryController();
        bool isMountSoftwareParked();

        // Park state of the ACTIVE_TELESCOPE as last reported by its driver.
        // Unknown (never reported, disconnected, busy, alert) is treated as NOT parked.
        bool mountParkKnown = false;
        bool mountParked    = false;

        IText CurrentStateT[1] {};
        ITextVectorProperty CurrentStateTP;

        IText CurrentParkT[1] {};
        ITextVectorProperty CurrentParkTP;

        ISState fullOpenLimitSwitch;
        ISState fullClosedLimitSwitch;
        bool bTelescopeParkingDEC;
        bool bTelescopeParkingRA;

        double MotionRequest;
        struct timeval MotionStart;

        float CalcTimeLeft(timeval);

        Firmata* sf = nullptr;
};

#endif
