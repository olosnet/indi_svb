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

#include <map>
#include "svb_ccd.h"

static class Loader
{
        INDI::Timer hotPlugTimer;
        std::map<int, std::shared_ptr<SVBCCD>> cameras;
    public:
        Loader()
        {
            load(false);

            // JM 2021-04-03: Some users reported camera dropping out since hotplug was introduced.
            // Disabling it for now until more investigation is conduced.
            //            hotPlugTimer.start(1000);
            //            hotPlugTimer.callOnTimeout([&]
            //            {
            //                if (getCountOfConnectedCameras() != cameras.size())
            //                {
            //                    load(true);
            //                }
            //            });
        }

    public:
        static size_t getCountOfConnectedCameras()
        {
            return size_t(std::max(SVBGetNumOfConnectedCameras(), 0));
        }

        static std::vector<SVB_CAMERA_INFO> getConnectedCameras()
        {
            std::vector<SVB_CAMERA_INFO> result(getCountOfConnectedCameras());
            int i = 0;
            for(auto &cameraInfo : result)
                SVBGetCameraInfo(&cameraInfo, i++);
            return result;
        }

    public:
        void load(bool isHotPlug)
        {
            auto usedCameras = std::move(cameras);

            UniqueName uniqueName(usedCameras);

            for(const auto &cameraInfo : getConnectedCameras())
            {
                int id = cameraInfo.CameraID;

                // camera already created
                if (usedCameras.find(id) != usedCameras.end())
                {
                    std::swap(cameras[id], usedCameras[id]);
                    continue;
                }

                SVBCCD *svbCCD = new SVBCCD(cameraInfo, uniqueName.make(cameraInfo));
                cameras[id] = std::shared_ptr<SVBCCD>(svbCCD);
                if (isHotPlug)
                    svbCCD->ISGetProperties(nullptr);
            }
        }

    public:
        class UniqueName
        {
                std::map<std::string, bool> used;
            public:
                UniqueName() = default;
                UniqueName(const std::map<int, std::shared_ptr<SVBCCD>> &usedCameras)
                {
                    for (const auto &camera : usedCameras)
                        used[camera.second->getDeviceName()] = true;
                }

                std::string make(const SVB_CAMERA_INFO &cameraInfo)
                {
                    std::string cameraName = "SVBONY " + std::string(cameraInfo.FriendlyName);
                    std::string uniqueName = cameraName;

                    for (int index = 0; used[uniqueName] == true; )
                        uniqueName = cameraName + " " + std::to_string(++index);

                    used[uniqueName] = true;
                    return uniqueName;
                }
        };
} loader;

///////////////////////////////////////////////////////////////////////
/// Constructor for multi-camera driver.
///////////////////////////////////////////////////////////////////////
SVBCCD::SVBCCD(const SVB_CAMERA_INFO &camInfo, const std::string &cameraName) : SVBDevice()
{
    mCameraName = cameraName;
    mCameraInfo = camInfo;
    setDeviceName(cameraName.c_str());
}
