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

#pragma once
#include <indiccd.h>
#include "indipropertytext.h"

#include "libsv305/SVBCameraSDK.h"


class SVBBase: public INDI::CCD
{

    public:

        SVBBase();
        virtual ~SVBBase() override;

        virtual const char* getDefaultName() override;

        virtual bool Connect() override;
        virtual bool Disconnect() override;

        virtual void ISGetProperties(const char *dev) override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        bool updateCCDParams();


        /** Get initial parameters from camera */
        void setupParams();

        /** Create number and switch controls for camera by querying the API */
        virtual bool createControls(int piNumberOfControls);

        /** Update control */
        bool updateControl(int ControlType, SVB_CONTROL_TYPE SVB_Control, double values[], char *names[], int n);

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

        // Save config
        virtual bool saveConfigItems(FILE *fp) override;

        #if INDI_VERSION_MAJOR >= 1 && INDI_VERSION_MINOR >= 9 && INDI_VERSION_RELEASE >=7
            virtual void addFITSKeywords(INDI::CCDChip *targetChip) override;
        #else
            virtual void addFITSKeywords(fitsfile *fptr, INDI::CCDChip *targetChip) override;
        #endif

        // Guide Port
        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;


    protected:

        std::string mCameraName, mCameraID;
        SVB_CAMERA_INFO mCameraInfo;
        SVB_CAMERA_PROPERTY cameraProperty;
        SVB_IMG_TYPE mCurrentVideoFormat;

        // ROI offsets
        int x_offset;
	    int y_offset;

        // exposure limits
        double minExposure;
        double maxExposure;

        // pixel size
        float pixelSize;

        // stretch factor x2, x4, x8, x16 (bit shift)
        int bitStretch;
        int bitDepth;
        ISwitch StretchS[5];
        ISwitchVectorProperty StretchSP;
        enum { STRETCH_OFF, STRETCH_X2, STRETCH_X4, STRETCH_X8, STRETCH_X16 };

        // controls settings
        enum
        {
            CCD_GAIN_N,
            CCD_CONTRAST_N,
            CCD_SHARPNESS_N,
            CCD_SATURATION_N,
            CCD_WBR_N,
            CCD_WBG_N,
            CCD_WBB_N,
            CCD_GAMMA_N,
            CCD_DOFFSET_N
        };
        INumber ControlsN[9];
        INumberVectorProperty ControlsNP[9];

        INDI::PropertySwitch  WorkaroundExpSP {2};
        INDI::PropertyNumber  WorkaroundExpNP {1};

        // frame speed
        ISwitch SpeedS[3];
        ISwitchVectorProperty SpeedSP;
        enum { SPEED_SLOW, SPEED_NORMAL, SPEED_FAST};
        int frameSpeed;

        // SDK Version
        INDI::PropertyText SDKVersionSP {1};

        // output frame format
        // the camera is able to output RGB24, but not supported by INDI
        // -> ignored
	    // NOTE : SV305M PRO doesn't support RAW8 and RAW16, only Y8 and Y16
        ISwitch FormatS[2];
        ISwitchVectorProperty FormatSP;
        enum { FORMAT_RAW16, FORMAT_RAW8, FORMAT_Y16, FORMAT_Y8};
        SVB_IMG_TYPE frameFormatMapping[4] = {SVB_IMG_RAW16, SVB_IMG_RAW8, SVB_IMG_Y16, SVB_IMG_Y8};
        int frameFormat;
        const char* bayerPatternMapping[4] = {"RGGB", "BGGR", "GRBG", "GBRG"};


        // Exposure workaround
        bool exposureWorkaroundEnable = false;
        float exposureWorkaroundDuration = 0.5F;

};
