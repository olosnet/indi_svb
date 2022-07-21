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

#include "svb_ccd.h"
#include "svb_helpers.h"

#include "config.h"

#include <stream/streammanager.h>
#include <indielapsedtimer.h>

#include <algorithm>
#include <cmath>
#include <vector>
#include <map>
#include <unistd.h>

#define MAX_EXP_RETRIES 3
#define VERBOSE_EXPOSURE 3

SVBDevice::SVBDevice()
{
    SVBTemperature();
}

SVBDevice::~SVBDevice()
{
    mWorker.quit();
}

void SVBDevice::workerStreamVideo(const std::atomic_bool &isAboutToQuit)
{
    LOG_INFO("framing\n");

    // stream init
    // NOTE : SV305M is MONO
    // if binning, no more bayer
    if (strcmp(mCameraInfo.FriendlyName, "SVBONY SV305M PRO") == 0 || isBinningActive())
    {
        Streamer->setPixelFormat(INDI_MONO, bitDepth);
    }
    else
    {
        Streamer->setPixelFormat(INDI_BAYER_GRBG, bitDepth);
    }
    Streamer->setSize(PrimaryCCD.getSubW() / PrimaryCCD.getBinX(), PrimaryCCD.getSubH() / PrimaryCCD.getBinY());

    double ExposureRequest = 1.0 / Streamer->getTargetFPS();
    long uSecs = static_cast<long>(ExposureRequest * 950000.0);

    // stop camera
    auto ret = SVBStopVideoCapture(mCameraInfo.CameraID);
    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, stop camera failed  (%s).", Helpers::toString(ret));
    }

    ret = SVBSetControlValue(mCameraInfo.CameraID, SVB_EXPOSURE, uSecs, SVB_FALSE);
    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Failed to set exposure duration (%s).", Helpers::toString(ret));
    }

    // set ROI back
    ret = SVBSetROIFormat(mCameraInfo.CameraID, x_offset, y_offset, PrimaryCCD.getSubW(), PrimaryCCD.getSubH(), 1);
    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, camera set subframe failed (%s).", Helpers::toString(ret));
    }
    LOG_INFO("Subframe set\n");

    // set camera normal mode
    ret = SVBSetCameraMode(mCameraInfo.CameraID, SVB_MODE_NORMAL);
    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, camera normal mode failed (%s).", Helpers::toString(ret));
    }
    LOG_INFO("Camera normal mode\n");

    ret = SVBStartVideoCapture(mCameraInfo.CameraID);
    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Failed to start video capture (%s).", Helpers::toString(ret));
    }

    while (!isAboutToQuit)
    {
        uint8_t *imageBuffer = PrimaryCCD.getFrameBuffer();
        uint32_t totalBytes = PrimaryCCD.getFrameBufferSize();
        int waitMS = static_cast<int>((ExposureRequest * 2000.0) + 500);

        std::unique_lock<std::mutex> guard(ccdBufferLock);
        ret = SVBGetVideoData(mCameraInfo.CameraID, imageBuffer, totalBytes, waitMS);
        if (ret != SVB_SUCCESS)
        {
            if (ret != SVB_ERROR_TIMEOUT)
            {
                Streamer->setStream(false);
                LOGF_ERROR("Failed to read video data (%s).", Helpers::toString(ret));
                break;
            }

            usleep(100);
            continue;
        }

        // stretching 12bits depth to 16bits depth
        if (bitDepth == 16 && (bitStretch != 0))
        {
            u_int16_t *tmp = (u_int16_t *)imageBuffer;
            for (int i = 0; i < totalBytes / 2; i++)
            {
                tmp[i] <<= bitStretch;
            }
        }

        if (isBinningActive())
        {
            PrimaryCCD.binFrame();
        }

        Streamer->newFrame(imageBuffer, totalBytes);
        guard.unlock();
    }
}

bool SVBDevice::StartStreaming()
{
    mWorker.start(std::bind(&SVBDevice::workerStreamVideo, this, std::placeholders::_1));
    return true;
}

bool SVBDevice::StopStreaming()
{
    mWorker.quit();
    LOG_INFO("stop framing\n");

    resetCaptureModeAndRoi(SVB_MODE_TRIG_SOFT);

    return true;
}

void SVBDevice::resetCaptureModeAndRoi(SVB_CAMERA_MODE mode)
{
    // stop camera
    auto status = SVBStopVideoCapture(mCameraInfo.CameraID);
    if (status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, stop camera failed (%s).", Helpers::toString(status));
    }

    // set camera back to trigger mode
    status = SVBSetCameraMode(mCameraInfo.CameraID, mode);
    if (status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, camera mode %d failed (%s).", mode, Helpers::toString(status));
    }
    LOG_INFO("Camera soft trigger mode\n");

    // set ROI back
    status = SVBSetROIFormat(mCameraInfo.CameraID, x_offset, y_offset, PrimaryCCD.getSubW(), PrimaryCCD.getSubH(), 1);
    if (status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, camera set subframe failed (%s).", Helpers::toString(status));
    }
    LOG_INFO("Subframe set\n");

    // start camera
    status = SVBStartVideoCapture(mCameraInfo.CameraID);
    if (status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, start camera failed (%s).", Helpers::toString(status));
    }
}

void SVBDevice::workerExposure(const std::atomic_bool &isAboutToQuit, float duration)
{   

    if (exposureWorkaroundEnable && exposureWorkaroundDuration > 0)
        workaroundExposure(0.5);

    PrimaryCCD.setExposureDuration(duration);

    LOGF_DEBUG("StartExposure->setexp : %.3fs", duration);

    auto ret = SVBSetControlValue(mCameraInfo.CameraID, SVB_EXPOSURE, duration * 1000 * 1000, SVB_FALSE);
    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Failed to set exposure duration (%s).", Helpers::toString(ret));
        PrimaryCCD.setExposureFailed();
        return;
    }

    if (duration > VERBOSE_EXPOSURE)
        LOGF_INFO("Taking a %g seconds frame...", duration);

    /*
    ret = SVBSetControlValue(mCameraInfo.CameraID, SVB_BLACK_LEVEL, 30, SVB_FALSE);
    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Failed to set offset (%s).", Helpers::toString(ret));
        PrimaryCCD.setExposureFailed();
        return;
    }
    */

    ret = SVBSendSoftTrigger(mCameraInfo.CameraID);
    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Failed to send soft trigger (%s).", Helpers::toString(ret));
        PrimaryCCD.setExposureFailed();
        return;
    }

    SVB_ERROR_CODE status;
    uint8_t *imageBuffer = nullptr;

    usleep(duration * 1000 * 1000);
    do
    {
        if (isAboutToQuit)
            return;
        imageBuffer = PrimaryCCD.getFrameBuffer();
        std::unique_lock<std::mutex> guard(ccdBufferLock);
        status = SVBGetVideoData(mCameraInfo.CameraID, imageBuffer, PrimaryCCD.getFrameBufferSize(), 100);
        guard.unlock();

        if (ret != SVB_SUCCESS && ret != SVB_ERROR_TIMEOUT)
        {
            LOGF_ERROR("Exposure failed, status %d (%s).", ret, Helpers::toString(ret));
            PrimaryCCD.setExposureFailed();
            return;
        }
    } while (status != SVB_SUCCESS);

    PrimaryCCD.setExposureLeft(0.0);
    LOG_INFO("Exposure done, downloading image...");

    // stretching 12bits depth to 16bits depth
    if (bitDepth == 16 && (bitStretch != 0))
    {
        u_int16_t *tmp = (u_int16_t *)imageBuffer;
        for (int i = 0; i < PrimaryCCD.getFrameBufferSize() / 2; i++)
        {
            tmp[i] <<= bitStretch;
        }
    }

    // binning if needed
    if (isBinningActive())
        PrimaryCCD.binFrame();

    // exposure done
    ExposureComplete(&PrimaryCCD);

    long currentValue;
    SVB_BOOL bauto;
    status = SVBGetControlValue(mCameraInfo.CameraID, SVB_BLACK_LEVEL, &currentValue, &bauto);
    if (status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, camera get %s failed (%s).", Helpers::toString(SVB_BLACK_LEVEL), Helpers::toString(status));
    }

    LOGF_INFO("Current offset: %d", currentValue);

    inExposure = false;
}

void SVBDevice::workaroundExposure(float duration)
{
    long uSecs = static_cast<long>(duration * 1000 * 1000);
    int waitMS = static_cast<int>((duration * 2000.0) + 500);

    // stop camera
    auto ret = SVBStopVideoCapture(mCameraInfo.CameraID);

    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, stop camera failed  (%s).", Helpers::toString(ret));
    }

    ret = SVBSetControlValue(mCameraInfo.CameraID, SVB_EXPOSURE, uSecs, SVB_FALSE);
    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Failed to set exposure duration (%s).", Helpers::toString(ret));
    }

    // set camera normal mode
    ret = SVBSetCameraMode(mCameraInfo.CameraID, SVB_MODE_NORMAL);
    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, camera normal mode failed (%s).", Helpers::toString(ret));
    }
    LOG_INFO("Camera normal mode\n");

    // set ROI back
    ret = SVBSetROIFormat(mCameraInfo.CameraID, x_offset, y_offset, PrimaryCCD.getSubW(), PrimaryCCD.getSubH(), 1);
    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, camera set subframe failed (%s).", Helpers::toString(ret));
    }
    LOG_INFO("Subframe set\n");

    ret = SVBStartVideoCapture(mCameraInfo.CameraID);
    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Failed to start video capture (%s).", Helpers::toString(ret));
    }

    // set ROI back
    ret = SVBSetROIFormat(mCameraInfo.CameraID, x_offset, y_offset, PrimaryCCD.getSubW(), PrimaryCCD.getSubH(), 1);
    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, camera set subframe failed (%s).", Helpers::toString(ret));
    }
    LOG_INFO("Subframe set\n");

    ret = SVBStartVideoCapture(mCameraInfo.CameraID);
    if (ret != SVB_SUCCESS)
    {
        LOGF_ERROR("Failed to start video capture (%s).", Helpers::toString(ret));
    }

    LOG_INFO("Workaround exposure in progress...");

    usleep(uSecs);
    do
    {
        std::unique_lock<std::mutex> guard(ccdBufferLock);
        ret = SVBGetVideoData(mCameraInfo.CameraID, nullptr, PrimaryCCD.getFrameBufferSize(), waitMS);
        guard.unlock();

        if (ret != SVB_SUCCESS && ret != SVB_ERROR_TIMEOUT)
        {
            LOGF_ERROR("Workaround Exposure failed, status %d (%s).", ret, Helpers::toString(ret));
            PrimaryCCD.setExposureFailed();
            return;
        }

        usleep(100);
    } while (ret != SVB_SUCCESS);

    resetCaptureModeAndRoi(SVB_MODE_TRIG_SOFT);
}

void SVBDevice::workerTimerExposure(const std::atomic_bool &isAboutToQuit, float duration)
{
    INDI::ElapsedTimer exposureTimer;

    do
    {
        float delay = 0.1;
        float timeLeft = std::max(duration - exposureTimer.elapsed() / 1000.0, 0.0);

        /*
         * Check the status every second until the time left is
         * about one second, after which decrease the poll interval
         *
         * For expsoures with more than a second left try
         * to keep the displayed "exposure left" value at
         * a full second boundary, which keeps the
         * count down neat
         */
        if (timeLeft > 1.1)
        {
            delay = std::max(timeLeft - std::trunc(timeLeft), 0.005f);
            timeLeft = std::round(timeLeft);
        }

        if (timeLeft > 0)
        {
            PrimaryCCD.setExposureLeft(timeLeft);
        }

        usleep(delay * 1000 * 1000);

        if (isAboutToQuit)
            return;

    } while (inExposure);
}

bool SVBDevice::StartExposure(float duration)
{
    float c_exp = duration;
    // checks for time limits
    if (c_exp < minExposure)
    {
        LOGF_WARN("Exposure shorter than minimum duration %g s requested. \n Setting exposure time to %g s.\n", duration,
                  minExposure);
        c_exp = minExposure;
    }

    if (c_exp > maxExposure)
    {
        LOGF_WARN("Exposure greater than minimum duration %g s requested. \n Setting exposure time to %g s.\n", duration,
                  maxExposure);
        c_exp = maxExposure;
    }

    inExposure = true;
    mWorker.start(std::bind(&SVBDevice::workerExposure, this, std::placeholders::_1, c_exp));
    mExposureTimerWorker.start(std::bind(&SVBDevice::workerTimerExposure, this, std::placeholders::_1, c_exp));

    return true;
}

bool SVBDevice::AbortExposure()
{
    LOG_INFO("Aborting exposure...");
    mWorker.quit();
    inExposure = false;
    mExposureTimerWorker.quit();

    LOG_INFO("Reset capture mode...");
    resetCaptureModeAndRoi(SVB_MODE_TRIG_SOFT);

    return true;
}

// subframing
bool SVBDevice::UpdateCCDFrame(int x, int y, int w, int h)
{
    if ((x + w) > cameraProperty.MaxWidth || (y + h) > cameraProperty.MaxHeight || w % 8 != 0 || h % 2 != 0)
    {
        LOG_ERROR("Error : Subframe out of range");
        return false;
    }

    // stop framing
    auto status = SVBStopVideoCapture(mCameraInfo.CameraID);
    if (status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, stop camera failed (%s)", Helpers::toString(status));
        return false;
    }

    // change ROI
    status = SVBSetROIFormat(mCameraInfo.CameraID, x, y, w, h, 1);
    if (status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, camera set subframe failed (%s)", Helpers::toString(status));
        return false;
    }
    LOG_INFO("Subframe set\n");

    // start framing
    status = SVBStartVideoCapture(mCameraInfo.CameraID);
    if (status != SVB_SUCCESS)
    {
        LOGF_ERROR("Error, start camera failed (%s)", Helpers::toString(status));
        return false;
    }

    x_offset = x;
    y_offset = y;

    return INDI::CCD::UpdateCCDFrame(x, y, w, h);
}

bool SVBDevice::UpdateCCDBin(int hor, int ver)
{
    INDI_UNUSED(ver);
    PrimaryCCD.setBin(hor, hor);

    return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

bool SVBDevice::isBinningActive()
{
    return PrimaryCCD.getBinX() > 1;
}
