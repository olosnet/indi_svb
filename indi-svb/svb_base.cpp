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

#include "config.h"
#include "svb_base.h"
#include "svb_helpers.h"
#include <unistd.h>

SVBBase::SVBBase()
{
    setVersion(SVB_VERSION_MAJOR, SVB_VERSION_MINOR);
}

SVBBase::~SVBBase()
{
    if (isConnected())
    {
        Disconnect();
    }
}


const char *SVBBase::getDefaultName()
{
    return "SVBONY CCD";
}


bool SVBBase::Connect()
{
    LOGF_INFO("Attempting to open %s...", mCameraName.c_str());

    SVB_ERROR_CODE status = SVB_SUCCESS;

    if (isSimulation() == false)
        status = SVBOpenCamera(mCameraInfo.CameraID);

    if (status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error connecting to the CCD (%s).", Helpers::toString(status));
        return false;
    }

    // wait a bit for the camera to get ready
    usleep(0.5 * 1e6);

    // get camera properties
    status = SVBGetCameraProperty(mCameraInfo.CameraID, &cameraProperty);
    if (status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, get camera property failed (%s).", Helpers::toString(status));
        return false;
    }

    // get camera pixel size
    status = SVBGetSensorPixelSize(mCameraInfo.CameraID, &pixelSize);
    if (status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, get camera pixel size failed (%s).", Helpers::toString(status));
        return false;
    }

    // get num of controls
    int controlsNum = 0;
    status          = SVBGetNumOfControls(mCameraInfo.CameraID, &controlsNum);
    if (status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, get camera controls failed (%s)", Helpers::toString(status));
        return false;
    }

    status = SVBSetAutoSaveParam(mCameraInfo.CameraID, SVB_FALSE);
    if (status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, set autosave param failed (%s)", Helpers::toString(status));
    }


    // fix for SDK gain error issue
    // set exposure time
    SVBSetControlValue(mCameraInfo.CameraID, SVB_EXPOSURE, (long)(1 * 1000000L), SVB_FALSE);

    // Create controls
    auto r = createControls(controlsNum);
    if(!r) return false;

    // set camera ROI and BIN
    SetCCDParams(cameraProperty.MaxWidth, cameraProperty.MaxHeight, bitDepth, pixelSize, pixelSize);
    status = SVBSetROIFormat(mCameraInfo.CameraID, 0, 0, cameraProperty.MaxWidth, cameraProperty.MaxHeight, 1);
    if (status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, camera set ROI failed (%s)", Helpers::toString(status));
        return false;
    }

    x_offset = 0;
    y_offset = 0;
    LOG_INFO("Camera set ROI\n");

    // set camera soft trigger mode
    status = SVBSetCameraMode(mCameraInfo.CameraID, SVB_MODE_TRIG_SOFT);
    if (status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, camera soft trigger mode failed (%s).", Helpers::toString(status));
        return false;
    }
    LOG_INFO("Camera soft trigger mode\n");

    // start framing
    status = SVBStartVideoCapture(mCameraInfo.CameraID);
    if (status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, start camera failed (%s).", Helpers::toString(status));
        return false;
    }

    // set CCD up
    updateCCDParams();

    /* Success! */
    LOG_INFO("CCD is online. Retrieving basic data.\n");
    return true;
}

bool SVBBase::Disconnect()
{
    // Save all config before shutdown
    saveConfig(true);

    LOGF_DEBUG("Closing %s...", mCameraName.c_str());

    Streamer->setStream(false);

    if (isSimulation() == false)
    {
        SVBStopVideoCapture(mCameraInfo.CameraID);
        SVBCloseCamera(mCameraInfo.CameraID);
    }

    LOG_INFO("CCD is offline.\n");

    setConnected(false, IPS_IDLE);
    return true;
}

void SVBBase::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);
}

bool SVBBase::initProperties()
{
    INDI::CCD::initProperties();

    // base capabilities
    uint32_t cap = CCD_CAN_ABORT | CCD_CAN_SUBFRAME | CCD_CAN_BIN | CCD_HAS_STREAMING;

    // SV305 is a color camera
    if (strcmp(mCameraInfo.FriendlyName, "SVBONY SV305") == 0)
    {
        cap |= CCD_HAS_BAYER;
    }

    // SV305 Pro is a color camera and has an ST4 port
    if (strcmp(mCameraInfo.FriendlyName, "SVBONY SV305PRO") == 0)
    {
        cap |= CCD_HAS_BAYER;
        cap |= CCD_HAS_ST4_PORT;
    }

    // SV305M Pro is a mono camera and has an ST4 port
    if (strcmp(mCameraInfo.FriendlyName, "SVBONY SV305M PRO") == 0)
    {
        cap |= CCD_HAS_ST4_PORT;
    }

    // SV905C is a color camera and has an ST4 port
    if (strcmp(mCameraInfo.FriendlyName, "SVBONY SV905C") == 0)
    {
        cap |= CCD_HAS_BAYER;
        cap |= CCD_HAS_ST4_PORT;
    }

    // SV405 CC is a cooled color camera
    if (strcmp(mCameraInfo.FriendlyName, "SVBONY SV405CC") == 0)
    {
        cap |= (CCD_HAS_BAYER | CCD_HAS_COOLER);
    }
    SetCCDCapability(cap);

    addConfigurationControl();
    addDebugControl();
    return true;
}

bool SVBBase::updateProperties()
{
    // Set format first if connected.
    if (isConnected())
    {
        // N.B. AFAIK, there is no way to switch image formats.
        CaptureFormat format;
        if (GetCCDCapability() & CCD_HAS_BAYER)
        {
            format = { "INDI_RAW", "RAW", 16, true };
        }
        else
        {
            format = { "INDI_MONO", "Mono", 16, true };
        }
        addCaptureFormat(format);
    }

    INDI::CCD::updateProperties();

    if (isConnected())
    {
        // define controls
        defineProperty(&ControlsNP[CCD_GAIN_N]);
        defineProperty(&ControlsNP[CCD_CONTRAST_N]);
        defineProperty(&ControlsNP[CCD_SHARPNESS_N]);
        defineProperty(&ControlsNP[CCD_SATURATION_N]);
        defineProperty(&ControlsNP[CCD_WBR_N]);
        defineProperty(&ControlsNP[CCD_WBG_N]);
        defineProperty(&ControlsNP[CCD_WBB_N]);
        defineProperty(&ControlsNP[CCD_GAMMA_N]);
        defineProperty(&ControlsNP[CCD_DOFFSET_N]);


        // define frame format
        defineProperty(&FormatSP);
        // define frame rate
        defineProperty(&SpeedSP);

        // stretch factor
        defineProperty(&StretchSP);
        // SDK version
        defineProperty(SDKVersionSP);

        // Workaround settings
        defineProperty(WorkaroundExpSP);
        defineProperty(WorkaroundExpNP);
    }
    else
    {

        // delete controls
        deleteProperty(ControlsNP[CCD_GAIN_N].name);
        deleteProperty(ControlsNP[CCD_CONTRAST_N].name);
        deleteProperty(ControlsNP[CCD_SHARPNESS_N].name);
        deleteProperty(ControlsNP[CCD_SATURATION_N].name);
        deleteProperty(ControlsNP[CCD_WBR_N].name);
        deleteProperty(ControlsNP[CCD_WBG_N].name);
        deleteProperty(ControlsNP[CCD_WBB_N].name);
        deleteProperty(ControlsNP[CCD_GAMMA_N].name);
        deleteProperty(ControlsNP[CCD_DOFFSET_N].name);

        // delete frame format
        deleteProperty(FormatSP.name);
        // delete frame rate
        deleteProperty(SpeedSP.name);

        // stretch factor
        deleteProperty(StretchSP.name);

        // sdk version
        deleteProperty(SDKVersionSP.getName());

        // Workaround settings
        deleteProperty(WorkaroundExpSP.getName());
        deleteProperty(WorkaroundExpNP.getName());


    }

    return true;
}

bool SVBBase::createControls(int piNumberOfControls)
{
    SVB_ERROR_CODE status;

    // read controls and feed UI
    for (int i = 0; i < piNumberOfControls; i++)
    {
        // read control
        SVB_CONTROL_CAPS caps;
        status = SVBGetControlCaps(mCameraInfo.CameraID, i, &caps);
        if (status != SVB_SUCCESS)
        {
            LOGF_ERROR("Error, get camera controls caps failed (%s), index: %d.", Helpers::toString(status), i);
            return false;
        }
        switch (caps.ControlType)
        {
            case SVB_EXPOSURE:
                // Exposure
                minExposure = (double)caps.MinValue / 1000000.0;
                maxExposure = (double)caps.MaxValue / 1000000.0;
                PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", minExposure, maxExposure, 1, true);
                break;

            case SVB_GAIN:
                // Gain
                IUFillNumber(&ControlsN[CCD_GAIN_N], "GAIN", "Gain", "%.f", caps.MinValue, caps.MaxValue, 10,
                             caps.DefaultValue);
                IUFillNumberVector(&ControlsNP[CCD_GAIN_N], &ControlsN[CCD_GAIN_N], 1, getDeviceName(), "CCD_GAIN",
                                   "Gain", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                status = SVBSetControlValue(mCameraInfo.CameraID, SVB_GAIN, caps.DefaultValue, SVB_FALSE);
                if (status != SVB_SUCCESS)
                {
                    LOGF_ERROR("Error, camera set gain failed (%s).", Helpers::toString(status));
                }
                break;

            case SVB_CONTRAST:
                // Contrast
                IUFillNumber(&ControlsN[CCD_CONTRAST_N], "CONTRAST", "Contrast", "%.f", caps.MinValue, caps.MaxValue,
                             caps.MaxValue / 10, caps.DefaultValue);
                IUFillNumberVector(&ControlsNP[CCD_CONTRAST_N], &ControlsN[CCD_CONTRAST_N], 1, getDeviceName(),
                                   "CCD_CONTRAST", "Contrast", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                status = SVBSetControlValue(mCameraInfo.CameraID, SVB_CONTRAST, caps.DefaultValue, SVB_FALSE);
                if (status != SVB_SUCCESS)
                {
                    LOGF_ERROR("Error, camera set contrast failed (%s).", Helpers::toString(status));
                }
                break;

            case SVB_SHARPNESS:
                // Sharpness
                IUFillNumber(&ControlsN[CCD_SHARPNESS_N], "SHARPNESS", "Sharpness", "%.f", caps.MinValue, caps.MaxValue,
                             caps.MaxValue / 10, caps.DefaultValue);
                IUFillNumberVector(&ControlsNP[CCD_SHARPNESS_N], &ControlsN[CCD_SHARPNESS_N], 1, getDeviceName(),
                                   "CCD_SHARPNESS", "Sharpness", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                status = SVBSetControlValue(mCameraInfo.CameraID, SVB_SHARPNESS, caps.DefaultValue, SVB_FALSE);
                if (status != SVB_SUCCESS)
                {
                    LOGF_ERROR("Error, camera set sharpness failed (%s).", Helpers::toString(status));
                }
                break;

            case SVB_SATURATION:
                // Saturation
                IUFillNumber(&ControlsN[CCD_SATURATION_N], "SATURATION", "Saturation", "%.f", caps.MinValue,
                             caps.MaxValue, caps.MaxValue / 10, caps.DefaultValue);
                IUFillNumberVector(&ControlsNP[CCD_SATURATION_N], &ControlsN[CCD_SATURATION_N], 1, getDeviceName(),
                                   "CCD_SATURATION", "Saturation", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                status = SVBSetControlValue(mCameraInfo.CameraID, SVB_SATURATION, caps.DefaultValue, SVB_FALSE);
                if (status != SVB_SUCCESS)
                {
                    LOGF_ERROR("Error, camera set saturation failed (%s).", Helpers::toString(status));
                }
                break;

            case SVB_WB_R:
                // Red White Balance
                IUFillNumber(&ControlsN[CCD_WBR_N], "WBR", "Red White Balance", "%.f", caps.MinValue, caps.MaxValue,
                             caps.MaxValue / 10, caps.DefaultValue);
                IUFillNumberVector(&ControlsNP[CCD_WBR_N], &ControlsN[CCD_WBR_N], 1, getDeviceName(), "CCD_WBR",
                                   "Red White Balance", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                status = SVBSetControlValue(mCameraInfo.CameraID, SVB_WB_R, caps.DefaultValue, SVB_FALSE);
                if (status != SVB_SUCCESS)
                {
                    LOGF_ERROR("Error, camera set red WB failed(%s).", Helpers::toString(status));
                }
                break;

            case SVB_WB_G:
                // Green White Balance
                IUFillNumber(&ControlsN[CCD_WBG_N], "WBG", "Green White Balance", "%.f", caps.MinValue, caps.MaxValue,
                             caps.MaxValue / 10, caps.DefaultValue);
                IUFillNumberVector(&ControlsNP[CCD_WBG_N], &ControlsN[CCD_WBG_N], 1, getDeviceName(), "CCD_WBG",
                                   "Green White Balance", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                status = SVBSetControlValue(mCameraInfo.CameraID, SVB_WB_G, caps.DefaultValue, SVB_FALSE);
                if (status != SVB_SUCCESS)
                {
                    LOGF_ERROR("Error, camera set green WB failed (%s).", Helpers::toString(status));
                }
                break;

            case SVB_WB_B:
                // Blue White Balance
                IUFillNumber(&ControlsN[CCD_WBB_N], "WBB", "Blue White Balance", "%.f", caps.MinValue, caps.MaxValue,
                             caps.MaxValue / 10, caps.DefaultValue);
                IUFillNumberVector(&ControlsNP[CCD_WBB_N], &ControlsN[CCD_WBB_N], 1, getDeviceName(), "CCD_WBB",
                                   "Blue White Balance", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                status = SVBSetControlValue(mCameraInfo.CameraID, SVB_WB_B, caps.DefaultValue, SVB_FALSE);
                if (status != SVB_SUCCESS)
                {
                    LOGF_ERROR("Error, camera set blue WB failed (%s).", Helpers::toString(status));
                }
                break;

            case SVB_GAMMA:
                // Gamma
                IUFillNumber(&ControlsN[CCD_GAMMA_N], "GAMMA", "Gamma", "%.f", caps.MinValue, caps.MaxValue,
                             caps.MaxValue / 10, caps.DefaultValue);
                IUFillNumberVector(&ControlsNP[CCD_GAMMA_N], &ControlsN[CCD_GAMMA_N], 1, getDeviceName(), "CCD_GAMMA",
                                   "Gamma", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                status = SVBSetControlValue(mCameraInfo.CameraID, SVB_GAMMA, caps.DefaultValue, SVB_FALSE);
                if (status != SVB_SUCCESS)
                {
                    LOGF_ERROR("Error, camera set gamma failed (%s).", Helpers::toString(status));
                }
                break;

            case SVB_BLACK_LEVEL:
                // Dark Offset
                IUFillNumber(&ControlsN[CCD_DOFFSET_N], "OFFSET", "Offset", "%.f", caps.MinValue, caps.MaxValue,
                             caps.MaxValue / 10, caps.DefaultValue);
                IUFillNumberVector(&ControlsNP[CCD_DOFFSET_N], &ControlsN[CCD_DOFFSET_N], 1, getDeviceName(),
                                   "CCD_OFFSET", "Offset", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
                status = SVBSetControlValue(mCameraInfo.CameraID, SVB_BLACK_LEVEL, caps.DefaultValue, SVB_FALSE);
                if (status != SVB_SUCCESS)
                {
                    LOGF_ERROR("Error, camera set offset failed (%s).", Helpers::toString(status));
                }
                break;

            default:
                break;
        }
    }

    // set frame speed
    IUFillSwitch(&SpeedS[SPEED_SLOW], "SPEED_SLOW", "Slow", ISS_OFF);
    IUFillSwitch(&SpeedS[SPEED_NORMAL], "SPEED_NORMAL", "Normal", ISS_ON);
    IUFillSwitch(&SpeedS[SPEED_FAST], "SPEED_FAST", "Fast", ISS_OFF);
    IUFillSwitchVector(&SpeedSP, SpeedS, 3, getDeviceName(), "FRAME_RATE", "Frame rate", MAIN_CONTROL_TAB, IP_RW,
                        ISR_1OFMANY, 60, IPS_IDLE);
    frameSpeed = SPEED_NORMAL;
    status     = SVBSetControlValue(mCameraInfo.CameraID, SVB_FRAME_SPEED_MODE, SPEED_NORMAL, SVB_FALSE);
    if (status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, camera set frame speed failed (%s)", Helpers::toString(status));
        return false;
    }

    // set frame format and feed UI
    IUFillSwitch(&FormatS[FORMAT_RAW8], "FORMAT_RAW8", "Raw 8 bits", ISS_OFF);
    IUFillSwitch(&FormatS[FORMAT_RAW16], "FORMAT_RAW16", "Raw 16 bits", ISS_ON);
    IUFillSwitchVector(&FormatSP, FormatS, 2, getDeviceName(), "FRAME_FORMAT", "Frame Format", MAIN_CONTROL_TAB,
                        IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // NOTE : SV305M PRO only supports Y8 and Y16 frame format
    if (strcmp(mCameraInfo.FriendlyName, "SVBONY SV305M PRO") == 0)
    {
        status = SVBSetOutputImageType(mCameraInfo.CameraID, frameFormatMapping[FORMAT_Y16]);
    }
    else
    {
        IUSaveText(&BayerT[0], "0");
        IUSaveText(&BayerT[1], "0");
        IUSaveText(&BayerT[2], bayerPatternMapping[cameraProperty.BayerPattern]);
        status = SVBSetOutputImageType(mCameraInfo.CameraID, frameFormatMapping[FORMAT_RAW16]);
    }
    if (status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, camera set frame format failed (%s).", Helpers::toString(status));
        return false;
    }
    bitDepth    = 16;
    frameFormat = FORMAT_RAW16;
    LOG_INFO("Camera set frame format mode\n");

    // set bit stretching and feed UI
    IUFillSwitch(&StretchS[STRETCH_OFF], "STRETCH_OFF", "Off", ISS_ON);
    IUFillSwitch(&StretchS[STRETCH_X2], "STRETCH_X2", "x2", ISS_OFF);
    IUFillSwitch(&StretchS[STRETCH_X4], "STRETCH_X4", "x4", ISS_OFF);
    IUFillSwitch(&StretchS[STRETCH_X8], "STRETCH_X8", "x8", ISS_OFF);
    IUFillSwitch(&StretchS[STRETCH_X16], "STRETCH_X16", "x16", ISS_OFF);
    IUFillSwitchVector(&StretchSP, StretchS, 5, getDeviceName(), "STRETCH_BITS", "12 bits 16 bits stretch", MAIN_CONTROL_TAB,
                       IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    bitStretch = 0;

    SDKVersionSP[0].fill("VERSION", "Version", SVBGetSDKVersion());
    SDKVersionSP.fill(getDeviceName(), "SDK", "SDK", INFO_TAB, IP_RO, 60, IPS_IDLE);

    IUFillSwitch(&SpeedS[SPEED_SLOW], "SPEED_SLOW", "Slow", ISS_OFF);

    WorkaroundExpSP[0].fill("WORKAROUND_ON",  "ON",  ISS_OFF);
    WorkaroundExpSP[1].fill("WORKAROUND_OFF", "OFF", ISS_ON);
    WorkaroundExpSP.fill(getDeviceName(), "EXP_WOKAROUND", "ExpWorkaround", "Extra", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    WorkaroundExpNP[0].fill("WORKAROUND_DURATION", "Duration", "%.2f", 0.1,  60, 0.001, 0.5);
    WorkaroundExpNP.fill(getDeviceName(), "EXP_WOKAROUND_DURATION", "ExpWorkaround", "Extra", IP_RW, 60, IPS_IDLE);



    return true;

}

// helper : update camera control depending on control type
bool SVBBase::updateControl(int ControlType, SVB_CONTROL_TYPE SVB_Control, double values[], char *names[], int n)
{
    IUUpdateNumber(&ControlsNP[ControlType], values, names, n);

    // set control
    auto status = SVBSetControlValue(mCameraInfo.CameraID, SVB_Control, ControlsN[ControlType].value, SVB_FALSE);
    if(status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, camera set control %s failed (%s)", Helpers::toString(SVB_Control), Helpers::toString(status));
        return false;
    }
    LOGF_INFO("Camera control %s to %.f\n", Helpers::toString(SVB_Control), ControlsN[ControlType].value);

    // For debug purposes
    long currValue = 0;
    SVB_BOOL bauto;
    status = SVBGetControlValue(mCameraInfo.CameraID, SVB_Control, &currValue, &bauto);
    if(status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, camera get control %s failed (%s)", Helpers::toString(SVB_Control), Helpers::toString(status));
    }

    LOGF_INFO("%s current value: %d, auto: %d", Helpers::toString(SVB_Control), currValue, bauto);


    ControlsNP[ControlType].s = IPS_OK;
    IDSetNumber(&ControlsNP[ControlType], nullptr);
    return true;
}


bool SVBBase::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (strcmp(dev, getDeviceName()))
        return false;

    // look for gain settings
    if (!strcmp(name, ControlsNP[CCD_GAIN_N].name))
    {
        return updateControl(CCD_GAIN_N, SVB_GAIN, values, names, n);
    }

    // look for contrast settings
    if (!strcmp(name, ControlsNP[CCD_CONTRAST_N].name))
    {
        return updateControl(CCD_CONTRAST_N, SVB_CONTRAST, values, names, n);
    }

    // look for sharpness settings
    if (!strcmp(name, ControlsNP[CCD_SHARPNESS_N].name))
    {
        return updateControl(CCD_SHARPNESS_N, SVB_SHARPNESS, values, names, n);
    }

    // look for saturation settings
    if (!strcmp(name, ControlsNP[CCD_SATURATION_N].name))
    {
        return updateControl(CCD_SATURATION_N, SVB_SATURATION, values, names, n);
    }

    // look for red WB settings
    if (!strcmp(name, ControlsNP[CCD_WBR_N].name))
    {
        return updateControl(CCD_WBR_N, SVB_WB_R, values, names, n);
    }

    // look for green WB settings
    if (!strcmp(name, ControlsNP[CCD_WBG_N].name))
    {
        return updateControl(CCD_WBG_N, SVB_WB_G, values, names, n);
    }

    // look for blue WB settings
    if (!strcmp(name, ControlsNP[CCD_WBB_N].name))
    {
        return updateControl(CCD_WBB_N, SVB_WB_B, values, names, n);
    }

    // look for gamma settings
    if (!strcmp(name, ControlsNP[CCD_GAMMA_N].name))
    {
        return updateControl(CCD_GAMMA_N, SVB_GAMMA, values, names, n);
    }

    // look for dark offset settings
    if (!strcmp(name, ControlsNP[CCD_DOFFSET_N].name))
    {
        return updateControl(CCD_DOFFSET_N, SVB_BLACK_LEVEL, values, names, n);
    }

    if (WorkaroundExpNP.isNameMatch(name))
    {
        WorkaroundExpNP.update(values, names, n);
        WorkaroundExpNP.setState(IPS_OK);
        WorkaroundExpNP.apply();
        exposureWorkaroundDuration = WorkaroundExpNP[0].getValue();
        return true;
    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

//
bool SVBBase::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{

    SVB_ERROR_CODE status;

    // Make sure the call is for our device
    if (!strcmp(dev, getDeviceName()))
    {
        // Check if the call for BPP switch
        if (!strcmp(name, FormatSP.name))
        {
            // Find out which state is requested by the client
            const char *actionName = IUFindOnSwitchName(states, names, n);
            // If same state as actionName, then we do nothing
            int tmpFormat = IUFindOnSwitchIndex(&FormatSP);
            if (actionName &&
                !strcmp(actionName, FormatS[tmpFormat].name)) // skip strcmp if there are no selected swich.
            {
                LOGF_INFO("Frame format is already %s", FormatS[tmpFormat].label);
                FormatSP.s = IPS_IDLE;
                IDSetSwitch(&FormatSP, NULL);
                return true;
            }

            // Otherwise, let us update the switch state
            IUUpdateSwitch(&FormatSP, states, names, n);
            tmpFormat = IUFindOnSwitchIndex(&FormatSP);
            if (tmpFormat == -1)
            {
                tmpFormat = FORMAT_RAW16; // Set Frame Format as FORMAT_RAW16 if frameFromat is -1
            }

            // adjust frame format for SV305M
            if (strcmp(mCameraInfo.FriendlyName, "SVBONY SV305M PRO") == 0)
            {
                // offset format mapper to Y16 and Y8 modes
                tmpFormat += FORMAT_Y16;
            }

            // set new format
            status = SVBSetOutputImageType(mCameraInfo.CameraID, frameFormatMapping[tmpFormat]);
            if (status != SVB_SUCCESS)
            {
                LOGF_ERROR("Error, camera set frame format failed (%s)", Helpers::toString(status));
            }
            // set frame format back for SV305M
            if (strcmp(mCameraInfo.FriendlyName, "SVBONY SV305M PRO") == 0)
            {
                tmpFormat -= FORMAT_Y16;
            }
            LOGF_INFO("Frame format is now %s", FormatS[tmpFormat].label);

            frameFormat = tmpFormat;

            // pixel depth
            switch (frameFormat)
            {
                case FORMAT_RAW8:
                    bitDepth = 8;
                    break;
                case FORMAT_RAW16:
                    bitDepth = 16;
                    break;
                default:
                    frameFormat = FORMAT_RAW16; // Set frameFormat as FORMAT_RAW16 if frameFromat is unknown
                    bitDepth    = 16;
                    break;
            }
            // update CCD parameters
            updateCCDParams();

            FormatSP.s = IPS_OK;
            IDSetSwitch(&FormatSP, NULL);
            return true;
        }

        // Check if the call for frame rate switch
        if (!strcmp(name, SpeedSP.name))
        {
            // Find out which state is requested by the client
            const char *actionName = IUFindOnSwitchName(states, names, n);
            // If same state as actionName, then we do nothing
            int tmpSpeed = IUFindOnSwitchIndex(&SpeedSP);
            if (!strcmp(actionName, SpeedS[tmpSpeed].name))
            {
                LOGF_INFO("Frame rate is already %s", SpeedS[tmpSpeed].label);
                SpeedSP.s = IPS_IDLE;
                IDSetSwitch(&SpeedSP, NULL);
                return true;
            }

            // Otherwise, let us update the switch state
            IUUpdateSwitch(&SpeedSP, states, names, n);
            tmpSpeed = IUFindOnSwitchIndex(&SpeedSP);

            // set new frame rate
            status = SVBSetControlValue(mCameraInfo.CameraID, SVB_FRAME_SPEED_MODE, tmpSpeed, SVB_FALSE);
            if (status != SVB_SUCCESS)
            {
                LOGF_ERROR("Error, camera set frame rate failed (%s)", Helpers::toString(status));
            }
            LOGF_INFO("Frame rate is now %s", SpeedS[tmpSpeed].label);

            frameSpeed = tmpSpeed;

            SpeedSP.s = IPS_OK;
            IDSetSwitch(&SpeedSP, NULL);
            return true;
        }

        // Check if the 16 bist stretch factor
        if (!strcmp(name, StretchSP.name))
        {
            // Find out which state is requested by the client
            const char *actionName = IUFindOnSwitchName(states, names, n);
            // If same state as actionName, then we do nothing
            int tmpStretch = IUFindOnSwitchIndex(&StretchSP);
            if (!strcmp(actionName, StretchS[tmpStretch].name))
            {
                LOGF_INFO("Stretch factor is already %s", StretchS[tmpStretch].label);
                StretchSP.s = IPS_IDLE;
                IDSetSwitch(&StretchSP, NULL);
                return true;
            }

            // Otherwise, let us update the switch state
            IUUpdateSwitch(&StretchSP, states, names, n);
            tmpStretch = IUFindOnSwitchIndex(&StretchSP);

            LOGF_INFO("Stretch factor is now %s", StretchS[tmpStretch].label);

            bitStretch = tmpStretch;

            StretchSP.s = IPS_OK;
            IDSetSwitch(&StretchSP, NULL);
            return true;
        }

        // Exposure workaround enable
        if (WorkaroundExpSP.isNameMatch(name))
        {
            WorkaroundExpSP.update(states, names, n);
            WorkaroundExpSP.setState(IPS_OK);
            WorkaroundExpSP.apply();
            LOGF_INFO("State: %d", WorkaroundExpSP[0].getState());
            exposureWorkaroundEnable = WorkaroundExpSP[0].getState() == ISS_ON;
            return true;
        }

    }

    // If we did not process the switch, let us pass it to the parent class to process it
    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

bool SVBBase::saveConfigItems(FILE *fp)
{
    // Save CCD Config
    INDI::CCD::saveConfigItems(fp);

    // Controls
    IUSaveConfigNumber(fp, &ControlsNP[CCD_GAIN_N]);
    IUSaveConfigNumber(fp, &ControlsNP[CCD_DOFFSET_N]);
    IUSaveConfigNumber(fp, &ControlsNP[CCD_CONTRAST_N]);
    IUSaveConfigNumber(fp, &ControlsNP[CCD_SHARPNESS_N]);
    IUSaveConfigNumber(fp, &ControlsNP[CCD_SATURATION_N]);
    IUSaveConfigNumber(fp, &ControlsNP[CCD_WBR_N]);
    IUSaveConfigNumber(fp, &ControlsNP[CCD_WBG_N]);
    IUSaveConfigNumber(fp, &ControlsNP[CCD_WBB_N]);
    IUSaveConfigNumber(fp, &ControlsNP[CCD_GAMMA_N]);

    // Frame format
    IUSaveConfigSwitch(fp, &FormatSP);
    IUSaveConfigSwitch(fp, &SpeedSP);

    // bit stretching
    IUSaveConfigSwitch(fp, &StretchSP);

    return true;
}

IPState SVBBase::GuideNorth(uint32_t ms)
{
    auto status = SVBPulseGuide(mCameraInfo.CameraID, SVB_GUIDE_NORTH, ms);
    if (status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, camera guide North failed (%s)", Helpers::toString(status));
        return IPS_ALERT;
    }
    LOG_INFO("Guiding North\n");

    return IPS_OK;
}

IPState SVBBase::GuideSouth(uint32_t ms)
{
    auto status = SVBPulseGuide(mCameraInfo.CameraID, SVB_GUIDE_SOUTH, ms);
    if (status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, camera guide South failed (%s)", Helpers::toString(status));
        return IPS_ALERT;
    }
    LOG_INFO("Guiding South\n");

    return IPS_OK;
}

IPState SVBBase::GuideEast(uint32_t ms)
{
    auto status = SVBPulseGuide(mCameraInfo.CameraID, SVB_GUIDE_EAST, ms);
    if (status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, camera guide East failed (%s)", Helpers::toString(status));
        return IPS_ALERT;
    }
    LOG_INFO("Guiding East\n");

    return IPS_OK;

}

IPState SVBBase::GuideWest(uint32_t ms)
{
    auto status = SVBPulseGuide(mCameraInfo.CameraID, SVB_GUIDE_WEST, ms);
    if (status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, camera guide West failed (%s)", Helpers::toString(status));
        return IPS_ALERT;
    }
    LOG_INFO("Guiding North\n");

    return IPS_OK;
}

#if INDI_VERSION_MAJOR >= 1 && INDI_VERSION_MINOR >= 9 && INDI_VERSION_RELEASE >=7
void SVBBase::addFITSKeywords(INDI::CCDChip *targetChip)
#else
void SVBBase::addFITSKeywords(fitsfile *fptr, INDI::CCDChip *targetChip)
#endif
{
#if INDI_VERSION_MAJOR >= 1 && INDI_VERSION_MINOR >= 9 && INDI_VERSION_RELEASE >=7
    INDI::CCD::addFITSKeywords(targetChip);
#else
    INDI::CCD::addFITSKeywords(fptr, targetChip);
#endif

// to avoid build issues with old indi
#if INDI_VERSION_MAJOR >= 1 && INDI_VERSION_MINOR >= 9 && INDI_VERSION_RELEASE >=7
    auto fptr = *targetChip->fitsFilePointer();
#endif

    // report controls in FITS file
    int _status = 0;
    fits_update_key_dbl(fptr, "Gain", ControlsN[CCD_GAIN_N].value, 3, "Gain", &_status);
    fits_update_key_dbl(fptr, "Contrast", ControlsN[CCD_CONTRAST_N].value, 3, "Contrast", &_status);
    fits_update_key_dbl(fptr, "Sharpness", ControlsN[CCD_SHARPNESS_N].value, 3, "Sharpness", &_status);

    // NOTE : SV305M PRO is mono
    if(strcmp(mCameraInfo.FriendlyName, "SVBONY SV305M PRO") != 0)
    {
        fits_update_key_dbl(fptr, "Saturation", ControlsN[CCD_SATURATION_N].value, 3, "Saturation", &_status);
        fits_update_key_dbl(fptr, "Red White Balance", ControlsN[CCD_WBR_N].value, 3, "Red White Balance", &_status);
        fits_update_key_dbl(fptr, "Green White Balance", ControlsN[CCD_WBG_N].value, 3, "Green White Balance", &_status);
        fits_update_key_dbl(fptr, "Blue White Balance", ControlsN[CCD_WBB_N].value, 3, "Blue White Balance", &_status);
    }

    fits_update_key_dbl(fptr, "Gamma", ControlsN[CCD_GAMMA_N].value, 3, "Gamma", &_status);
    fits_update_key_dbl(fptr, "Frame Speed", frameSpeed, 3, "Frame Speed", &_status);
    fits_update_key_dbl(fptr, "Offset", ControlsN[CCD_DOFFSET_N].value, 3, "Offset", &_status);
    fits_update_key_dbl(fptr, "16 bits stretch factor (bit shift)", bitStretch, 3, "Stretch factor", &_status);
}

// set CCD parameters
bool SVBBase::updateCCDParams()
{
    // set CCD parameters
    PrimaryCCD.setBPP(bitDepth);

    // Let's calculate required buffer
    int nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
    PrimaryCCD.setFrameBufferSize(nbuf);

    LOGF_INFO("PrimaryCCD buffer size : %d\n", nbuf);

    return true;
}
