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
        virtual ~RollRoof();

        virtual bool initProperties();
        const char *getDefaultName();
        bool updateProperties();
        virtual bool ISSnoopDevice (XMLEle *root);
		virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
		virtual bool saveConfigItems(FILE *fp);

      protected:

        bool Connect();
        bool Disconnect();

        void TimerHit();

        virtual IPState Move(DomeDirection dir, DomeMotionCommand operation);
        virtual IPState Park();
        virtual IPState UnPark();
        virtual bool Abort();

        virtual bool getControllerState();
        virtual bool getFullOpenedLimitSwitch();
        virtual bool getFullClosedLimitSwitch();

    private:

        IText CurrentStateT[1];
        ITextVectorProperty CurrentStateTP;

        IText CurrentParkT[1];
        ITextVectorProperty CurrentParkTP;

        ISState fullOpenLimitSwitch;
        ISState fullClosedLimitSwitch;
        bool bTelescopeParkingDEC;
        bool bTelescopeParkingRA;

        double MotionRequest;
        struct timeval MotionStart;

        float CalcTimeLeft(timeval);

        Firmata* sf;
};

#endif
