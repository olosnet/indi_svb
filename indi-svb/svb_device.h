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

#include "indisinglethreadpool.h"

#include <vector>


#include "libsv305/SVBCameraSDK.h"
#include "svb_temperature.h"

class SingleWorker;
class SVBDevice: public SVBTemperature
{

    public:
        SVBDevice();
        ~SVBDevice() override;

        virtual bool StartExposure(float duration) override;
        virtual bool AbortExposure() override;

    protected:

        // Streaming
        virtual bool StartStreaming() override;
        virtual bool StopStreaming() override;

        virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
        virtual bool UpdateCCDBin(int binx, int biny) override;

    protected:
        INDI::SingleThreadPool mWorker;
        INDI::SingleThreadPool mExposureTimerWorker;
        void workerStreamVideo(const std::atomic_bool &isAboutToQuit);
        void workerExposure(const std::atomic_bool &isAboutToQuit, float duration);
        void workaroundExposure(float duration);
        void workerTimerExposure(const std::atomic_bool &isAboutToQuit, float duration);
    protected:

        void resetCaptureModeAndRoi(SVB_CAMERA_MODE mode);

        /** Return user selected image type */
        SVB_IMG_TYPE getImageType() const;

        /** Get is binning is active */
        bool isBinningActive();

    private:
        float lastDuration;
        bool inExposure;
};
