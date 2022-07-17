/*
    SVBONY CCD Driver

    Copyright (C) 2022 Valerio Faiuolo (valerio.faiuolo@gmail.com)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#pragma once
#include "svb_base.h"

class SVBTemperature: public SVBBase  {


    public:

        SVBTemperature();
        virtual ~SVBTemperature() override;

        virtual bool Connect() override;
        virtual bool Disconnect() override;

        virtual int SetTemperature(double temperature) override;

        virtual bool updateProperties() override;

        /** Create number and switch controls for camera by querying the API */
        bool createControls(int piNumberOfControls) override;

        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        void temperatureTimerTimeout();
        double mCurrentTemperature;
        INDI::Timer mTimerTemperature;

        double TemperatureRequest;
        int coolerEnable; // 0:Enable, 1:Disable
        ISwitch CoolerS[2];
        ISwitchVectorProperty CoolerSP;
        INumberVectorProperty CoolerNP;
        enum { COOLER_ENABLE = 0, COOLER_DISABLE = 1 };

        // cooler power
        INumber CoolerN[1];
};

