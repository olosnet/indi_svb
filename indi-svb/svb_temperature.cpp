/*
    SVBONY CCD Driver

    Copyright (C) 2022 Valerio Faiuolo (valerio.faiuolo@gmail.com)

    Based on indi-sv305 driver:
        - Jasem Mutlaq  (mutlaqja AT ikarustech DOT com) : generic-ccd skeleton
        - Blaise-Florentin Collin  (thx8411 AT yahoo DOT fr) : main coding
        - Tetsuya Kakura (jcpgm AT outlook DOT jp) : SV405CC support, fixes and code cleaning


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

#include "svb_temperature.h"
#include "svb_helpers.h"
#include "libsv305/SVBCameraSDK.h"

#define TEMP_TIMER_MS    1000 /* Temperature polling time (ms) */
#define TEMP_THRESHOLD   .25  /* Differential temperature threshold (C)*/


SVBTemperature::SVBTemperature()
{
    SVBBase();
}

SVBTemperature::~SVBTemperature()
{
}


///////////////////////////////////////////////////////////////////////////////////////
/// Set camera temperature
///////////////////////////////////////////////////////////////////////////////////////
int SVBTemperature::SetTemperature(double temperature)
{
    /**********************************************************
     *  We return 0 if setting the temperature will take some time
     *  If the requested is the same as current temperature, or very
     *  close, we return 1 and INDI::CCD will mark the temperature status as OK
     *  If we return 0, INDI::CCD will mark the temperature status as BUSY
     **********************************************************/
    SVB_ERROR_CODE ret;

    // If below threshold, do nothing
    if (std::abs(temperature - TemperatureN[0].value) < TemperatureRampNP[RAMP_THRESHOLD].value)
    {
        return 1; // The requested temperature is the same as current temperature, or very close
    }

    // Set target temperature
    if (SVB_SUCCESS !=
        (ret = SVBSetControlValue(mCameraInfo.CameraID, SVB_TARGET_TEMPERATURE, (long)(temperature * 10), SVB_FALSE)))
    {
        LOGF_ERROR("Setting target temperature %+06.2f, failed. (%s)", temperature, Helpers::toString(ret));
        return -1;
    }

    // Enable Cooler
    if (SVB_SUCCESS != (ret = SVBSetControlValue(mCameraInfo.CameraID, SVB_COOLER_ENABLE, 1, SVB_FALSE)))
    {
        LOGF_ERROR("Enabling cooler is fail (%s)", Helpers::toString(ret));
        return -1;
    }

    CoolerS[COOLER_ENABLE].s  = ISS_ON;
    CoolerS[COOLER_DISABLE].s = ISS_OFF;
    CoolerSP.s                = IPS_OK;
    IDSetSwitch(&CoolerSP, NULL);

    // Otherwise, we set the temperature request and we update the status in TimerHit() function.
    TemperatureRequest = temperature;
    LOGF_INFO("Setting CCD temperature to %+06.2f C", temperature);

    return 0;
}

void SVBTemperature::temperatureTimerTimeout()
{
    SVB_ERROR_CODE ret;
    SVB_BOOL isAuto = SVB_FALSE;
    long value = 0;
    IPState newState = TemperatureNP.s;

    ret = SVBGetControlValue(mCameraInfo.CameraID, SVB_CURRENT_TEMPERATURE, &value, &isAuto);

    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Failed to get temperature (%s).", Helpers::toString(ret));
        newState = IPS_ALERT;
    }
    else
    {
        mCurrentTemperature = value / 10.0;
    }

    // Update if there is a change
    if (
        std::abs(mCurrentTemperature - TemperatureN[0].value) > 0.05 ||
        TemperatureNP.s != newState
    )
    {
        TemperatureNP.s = newState;
        TemperatureN[0].value = mCurrentTemperature;
        IDSetNumber(&TemperatureNP, nullptr);
    }

    ret = SVBGetControlValue(mCameraInfo.CameraID, SVB_COOLER_POWER, &value, &isAuto);
    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, unable to get cooler power (%s).", Helpers::toString(ret));
        CoolerNP.s = IPS_ALERT;
    }
    else
    {
        CoolerN[0].value = (double)value;
        CoolerNP.s = value > 0 ? IPS_BUSY : IPS_IDLE;
    }

    IDSetNumber(&CoolerNP, nullptr);
}

bool SVBTemperature::updateProperties() {
    SVBBase::updateProperties();

    if (isConnected())
    {
        // cooler enable
        defineProperty(&CoolerSP);
        defineProperty(&CoolerNP);
    } 
    else {
        // delete cooler enable
        deleteProperty(CoolerSP.name);
        deleteProperty(CoolerNP.name);
    }

    return true;
}

bool SVBTemperature::createControls(int piNumberOfControls) {

    auto r = SVBBase::createControls(piNumberOfControls);
    SVB_ERROR_CODE status;

    LOG_INFO("Check cooler info");

    if (r) {
        // Cooler Enable
        if (HasCooler())
        {
            // set initial target temperature
            IUFillNumber(&TemperatureN[0], "CCD_TEMPERATURE_VALUE", "Temperature (C)", "%5.2f", -50.0, 50.0, 0., 25.);

            // default target temperature is 0. Setting to 25.
            if (SVB_SUCCESS !=
                (status = SVBSetControlValue(mCameraInfo.CameraID, SVB_TARGET_TEMPERATURE, (long)(25 * 10), SVB_FALSE)))
            {
                LOGF_ERROR("Setting default target temperature %d failed. (%s)", 25, status, Helpers::toString(status));
            }
            TemperatureRequest = 25;

            // set cooler status to disable
            IUFillSwitch(&CoolerS[COOLER_ENABLE], "COOLER_ON", "ON", ISS_OFF);
            IUFillSwitch(&CoolerS[COOLER_DISABLE], "COOLER_OFF", "OFF", ISS_ON);
            IUFillSwitchVector(&CoolerSP, CoolerS, 2, getDeviceName(), "CCD_COOLER", "Cooler", MAIN_CONTROL_TAB, IP_WO,
                               ISR_1OFMANY, 60, IPS_IDLE);

            // cooler power info
            IUFillNumber(&CoolerN[0], "CCD_COOLER_POWER_VALUE", "Cooler power (%)", "%3.f", 0.0, 100.0, 1., 0.);
            IUFillNumberVector(&CoolerNP, CoolerN, 1, getDeviceName(), "CCD_COOLER_POWER", "Cooler power",
                               MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);
        }
        coolerEnable = COOLER_DISABLE;
    }

    return r;
}

bool SVBTemperature::Connect() {

    auto r = SVBBase::Connect();

    if(r) {
        mTimerTemperature.callOnTimeout(std::bind(&SVBTemperature::temperatureTimerTimeout, this));
        mTimerTemperature.start(TEMP_TIMER_MS);
    }

    return r;
}

bool SVBTemperature::Disconnect() {
    mTimerTemperature.stop();
    return SVBBase::Disconnect();
}

bool SVBTemperature::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) {


    // Check if the Cooler Enable
    if (!strcmp(name, CoolerSP.name))
    {
        // Find out which state is requested by the client
        const char *actionName = IUFindOnSwitchName(states, names, n);
        // If same state as actionName, then we do nothing
        int tmpCoolerEnable = IUFindOnSwitchIndex(&CoolerSP);
        if (!strcmp(actionName, CoolerS[tmpCoolerEnable].name))
        {
            LOGF_INFO("Cooler Enable is already %s", CoolerS[tmpCoolerEnable].label);
            CoolerSP.s = IPS_IDLE;
            IDSetSwitch(&CoolerSP, NULL);
            return true;
        }

        // Otherwise, let us update the switch state
        IUUpdateSwitch(&CoolerSP, states, names, n);
        tmpCoolerEnable = IUFindOnSwitchIndex(&CoolerSP);

        LOGF_INFO("Cooler Power is now %s", CoolerS[tmpCoolerEnable].label);

        coolerEnable = tmpCoolerEnable;

        SVB_ERROR_CODE ret;
        // Change cooler state
        if (SVB_SUCCESS != (ret = SVBSetControlValue(mCameraInfo.CameraID, SVB_COOLER_ENABLE,
                                                        (coolerEnable == COOLER_ENABLE ? 1 : 0), SVB_FALSE)))
        {
            LOGF_INFO("Enabling cooler is fail.(SVB_COOLER_ENABLE:%d)", ret);
        }

        CoolerSP.s = IPS_OK;
        IDSetSwitch(&CoolerSP, NULL);
        return true;
    }

    // If we did not process the switch, let us pass it to the parent class to process it
    return SVBBase::ISNewSwitch(dev, name, states, names, n);
}