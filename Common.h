/*  -*- Mode: c++ -*-
 *
 *  Copyright (C) John Poet 2018
 *
 * This file is part of HauppaugeUSB.
 *
 * HauppaugeUSB is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HauppaugeUSB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HauppaugeUSB.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _Common_H_
#define _Common_H_

#include "encoderDev_DXT.h"
#include "HapiBaseType.h"
#include "HapiCommon.h"

#include <string>

struct Parameters
{
    std::string version;
    int    verbose;
    bool   audioBoost;
    int    tsBitrate;
    int    videoBitrate;
    int    videoVBRMin;
    int    videoVBRMax;
    int    bFrames;
    float  aspectRatio;
    std::string serial;
    std::string output;
    bool   mythtv;
    bool   flipFields;

    _HAPI_VIDEO_CAPTURE_SOURCE videoInput;
    _HAPI_AUDIO_CAPTURE_SOURCE audioInput;
    _HAPI_RATE_CONTROL     videoRateControl;
    _HAPI_CODING_MODE      videoCodingMode;
    _HAPI_VIDEO_PROFILE    videoProfile;
    _HAPI_VIDEO_H264_LEVEL videoH264Level;
    _HAPI_LATENCY          videoLatency;

    HAPI_AUDIO_CODEC       audioCodec;
#if 0
    _HAPI_AUDIO_CAPTURE_MODE audioCaptureMode;
#endif
    HAPI_AUDIO_SAMPLE_RATE audioSamplerate;
    _HAPI_AUDIO_BITRATE    audioBitrate;
};

bool StartEncoding(encoderDev_DXT_t & encDev, FX2Device_t & fx2);
bool StopEncoding(encoderDev_DXT_t & encDev, FX2Device_t & fx2);

#endif
