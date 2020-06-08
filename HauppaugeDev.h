/*  -*- Mode: c++ -*-
 *
 * Copyright (C) John Poet 2018
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

#ifndef HauppaugeDev_H_
#define HauppaugeDev_H_

#include "USBif.h"
#include "FX2Device.h"
#include "receiver_ADV7842.h"
#include "audio_CS8416.h"
#include "Common.h"

#include <string>
#include <thread>

class HauppaugeDev
{
  public:
    enum constants { MAX_RETRY = 300 };

    HauppaugeDev(const Parameters & params);
    ~HauppaugeDev(void);

    bool Open(USBWrapper_t & usbio, bool ac3,
              DataTransfer::callback_t * cb = nullptr);
    void Close(void);

    bool StartEncoding(void);
    bool StopEncoding(void);

    encoderDev_DXT_t &encDev(void) const { return *m_encDev; }
    FX2Device_t      &fx2(void) const { return *m_fx2; }

    std::string ErrorString(void) const { return m_errmsg; }
    bool operator!(void) const { return m_err; }

    audio_CS8416       *m_audio_CS8416;

    bool getInputAudioCodecChanged(HAPI_AUDIO_CODEC & audioCodec);
    bool setAudioMode(HAPI_AUDIO_CODEC audioCodec);

  protected:
    void configure(void);
    bool set_digital_audio(bool optical);
    bool set_audio_format(encoderAudioInFormat_t audioFormat);
    bool set_input_format(encoderSource_t source,
                          unsigned width, unsigned height,
                          bool interlaced, float vFreq,
                          float aspectRatio, float audioSampleRate);
    bool valid_resolution(int width, int height);
    bool init_cvbs(void);
    bool init_component(void);
    bool init_sdi(void);
    bool init_hdmi(void);
    bool open_file(const std::string & file_name);
    void log_ports(void);

    friend void * thread_start(void *);
    void audioMonitorLoop(void);
    HAPI_AUDIO_CODEC getAudioCodec(uint8_t format, uint8_t bppc0);

  private:
    int                 m_fd;

    receiver_ADV7842_t *m_rxDev;
    encoderDev_DXT_t   *m_encDev;
    FX2Device_t        *m_fx2;
    const Parameters   &m_params;
    int                 m_video_initialized;

    std::string         m_errmsg;
    bool                m_err;

    std::thread         m_audioMonitorThread;
    bool volatile       m_exitAudioMonitorLoop;
    HAPI_AUDIO_CODEC    m_currentAudioCodec;
};

#endif
