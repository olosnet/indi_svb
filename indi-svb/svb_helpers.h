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
#include "libsv305/SVBCameraSDK.h"

namespace Helpers
{

inline const char *toString(SVB_GUIDE_DIRECTION dir)
{
    switch (dir)
    {
    case SVB_GUIDE_NORTH: return "North";
    case SVB_GUIDE_SOUTH: return "South";
    case SVB_GUIDE_EAST:  return "East";
    case SVB_GUIDE_WEST:  return "West";
    default:              return "Unknown";
    }
}

inline const char *toString(SVB_BAYER_PATTERN pattern)
{
    switch (pattern)
    {
    case SVB_BAYER_BG: return "BGGR";
    case SVB_BAYER_GR: return "GRBG";
    case SVB_BAYER_GB: return "GBRG";
    default:           return "RGGB";
    }
}

inline const char *toString(SVB_ERROR_CODE code)
{
    switch (code)
    {

    case SVB_SUCCESS:                    return "SVB_SUCCESS";
    case SVB_ERROR_INVALID_INDEX:        return "SVB_ERROR_INVALID_INDEX";
    case SVB_ERROR_INVALID_ID:           return "SVB_ERROR_INVALID_ID";
    case SVB_ERROR_INVALID_CONTROL_TYPE: return "SVB_ERROR_INVALID_CONTROL_TYPE";
    case SVB_ERROR_CAMERA_CLOSED:        return "SVB_ERROR_CAMERA_CLOSED";
    case SVB_ERROR_CAMERA_REMOVED:       return "SVB_ERROR_CAMERA_REMOVED";
    case SVB_ERROR_INVALID_PATH:         return "SVB_ERROR_INVALID_PATH";
    case SVB_ERROR_INVALID_FILEFORMAT:   return "SVB_ERROR_INVALID_FILEFORMAT";
    case SVB_ERROR_INVALID_SIZE:         return "SVB_ERROR_INVALID_SIZE";
    case SVB_ERROR_INVALID_IMGTYPE:      return "SVB_ERROR_INVALID_IMGTYPE";
    case SVB_ERROR_OUTOF_BOUNDARY:       return "SVB_ERROR_OUTOF_BOUNDARY";
    case SVB_ERROR_TIMEOUT:              return "SVB_ERROR_TIMEOUT";
    case SVB_ERROR_INVALID_SEQUENCE:     return "SVB_ERROR_INVALID_SEQUENCE";
    case SVB_ERROR_BUFFER_TOO_SMALL:     return "SVB_ERROR_BUFFER_TOO_SMALL";
    case SVB_ERROR_VIDEO_MODE_ACTIVE:    return "SVB_ERROR_VIDEO_MODE_ACTIVE";
    case SVB_ERROR_EXPOSURE_IN_PROGRESS: return "SVB_ERROR_EXPOSURE_IN_PROGRESS";
    case SVB_ERROR_GENERAL_ERROR:        return "SVB_ERROR_GENERAL_ERROR";
    case SVB_ERROR_INVALID_MODE:         return "SVB_ERROR_INVALID_MODE";
    case SVB_ERROR_INVALID_DIRECTION:    return "SVB_ERROR_INVALID_DIRECTION";
    case SVB_ERROR_UNKNOW_SENSOR_TYPE:   return "SVB_ERROR_UNKNOW_SENSOR_TYPE";
    case SVB_ERROR_END:                  return "SVB_ERROR_END";

    }
    return "UNKNOWN";
}

inline const char *toString(SVB_IMG_TYPE type)
{
    switch (type)
    {
    case SVB_IMG_RAW8:   return "SVB_IMG_RAW8";
    case SVB_IMG_RAW10:  return "SVB_IMG_RAW10";
    case SVB_IMG_RAW12:  return "SVB_IMG_RAW12";
    case SVB_IMG_RAW14:  return "SVB_IMG_RAW14";
    case SVB_IMG_RAW16:  return "SVB_IMG_RAW16";
    case SVB_IMG_Y8:     return "SVB_IMG_Y8";
    case SVB_IMG_Y10:    return "SVB_IMG_Y10";
    case SVB_IMG_Y12:    return "SVB_IMG_Y12";
    case SVB_IMG_Y14:    return "SVB_IMG_Y14";
    case SVB_IMG_Y16:    return "SVB_IMG_Y16";
    case SVB_IMG_RGB24:  return "SVB_IMG_RGB24";
    case SVB_IMG_RGB32:  return "SVB_IMG_RGB32";
    default:             return "UNKNOWN";
    }
}

inline const char* toString(SVB_CONTROL_TYPE type)
{
    switch(type)
    {
	    case SVB_GAIN: return "SVB_GAIN";
	    case SVB_EXPOSURE: return "SVB_EXPOSURE";
	    case SVB_GAMMA: return "SVB_GAMMA";
	    case SVB_GAMMA_CONTRAST: return "SVB_GAMMA_CONTRAST";
	    case SVB_WB_R: return "SVB_WB_R";
	    case SVB_WB_G: return "SVB_WB_G";
	    case SVB_WB_B: return "SVB_WB_B";
	    case SVB_FLIP: return "SVB_FLIP";
	    case SVB_FRAME_SPEED_MODE: return "SVB_FRAME_SPEED_MODE";
	    case SVB_CONTRAST: return "SVB_CONTRAST";
	    case SVB_SHARPNESS: return "SVB_SHARPNESS";
	    case SVB_SATURATION: return "SVB_SATURATION";
	    case SVB_AUTO_TARGET_BRIGHTNESS: return "SVB_AUTO_TARGET_BRIGHTNESS";
	    case SVB_BLACK_LEVEL: return "SVB_BLACK_LEVEL";
	    case SVB_COOLER_ENABLE: return "SVB_COOLER_ENABLE";
	    case SVB_TARGET_TEMPERATURE: return "SVB_TARGET_TEMPERATURE";
	    case SVB_CURRENT_TEMPERATURE: return "SVB_CURRENT_TEMPERATURE";
	    case SVB_COOLER_POWER: return "SVB_COOLER_POWER";
        default:             return "UNKNOWN";
    }
}


inline const char *toPrettyString(SVB_IMG_TYPE type)
{
    switch (type)
    {
    case SVB_IMG_RAW8:   return "Raw 8 bit";
    case SVB_IMG_RAW10:  return "Raw 10 bit";
    case SVB_IMG_RAW12:  return "Raw 12 bit";
    case SVB_IMG_RAW14:  return "Raw 14 bit";
    case SVB_IMG_RAW16:  return "Raw 16 bit";
    case SVB_IMG_Y8:     return "Luma";
    case SVB_IMG_Y10:    return "Luma 10 bit";
    case SVB_IMG_Y12:    return "Luma 12 bit";
    case SVB_IMG_Y14:    return "Luma 14 bit";
    case SVB_IMG_Y16:    return "Luma 16 bit";
    case SVB_IMG_RGB24:  return "RGB 24";
    case SVB_IMG_RGB32:  return "RGB 32";
    default:             return "UNKNOWN";
    }
}


inline INDI_PIXEL_FORMAT pixelFormat(SVB_IMG_TYPE type, SVB_BAYER_PATTERN pattern, bool isColor)
{
    if (isColor == false)
        return INDI_MONO;

    switch (type)
    {
    case SVB_IMG_RGB24:
    case SVB_IMG_RGB32: return INDI_RGB;
    case SVB_IMG_Y8:
    case SVB_IMG_Y10:
    case SVB_IMG_Y12:
    case SVB_IMG_Y14:
    case SVB_IMG_Y16:   return INDI_MONO;
    default:;           // see below
    }

    switch (pattern)
    {
    case SVB_BAYER_RG: return INDI_BAYER_RGGB;
    case SVB_BAYER_BG: return INDI_BAYER_BGGR;
    case SVB_BAYER_GR: return INDI_BAYER_GRBG;
    case SVB_BAYER_GB: return INDI_BAYER_GBRG;
    }

    return INDI_MONO;
}



}