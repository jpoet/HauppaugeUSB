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

#define DECLARE_THREAD_START

#include <unistd.h>
#include <thread>
#include <math.h>
#include <iomanip>

#include "registryif.h"
#include "audio_CX2081x.h"
#include "device_EDID.h"
#include "hauppauge2_EDID.h"
//Common/Rx/audio_CS8416.h
#include "audio_CS8416.h"

#include "HauppaugeDev.h"
#include "FlipInterlacedFields.h"
#include "Logger.h"

using namespace std;

HauppaugeDev::HauppaugeDev(const Parameters & params)
    : m_audio_CS8416(nullptr)
    , m_fd(-1)
    , m_rxDev(nullptr)
    , m_encDev(nullptr)
    , m_fx2(nullptr)
    , m_params(params)
    , m_video_initialized(-1)
    , m_err(false)
{
    configure();
}

HauppaugeDev::~HauppaugeDev(void)
{
    Close();
}

void HauppaugeDev::configure(void)
{
#if 0
    switch (m_params.audio_rate)
    {
        case HAPI_AUDIO_SAMPLE_RATE_FOLLOW_INPUT:
          m_params.audioSamplerate = -1;
          break;
        case HAPI_AUDIO_SAMPLE_RATE_NONE:
          m_params.audioSamplerate = 0;
          break;
        case HAPI_AUDIO_SAMPLE_RATE_32_KHZ:
          m_params.audioSamplerate = 32000;
          break;
        case HAPI_AUDIO_SAMPLE_RATE_44_1_KHZ:
          m_params.audioSamplerate = 44100;
          break;
        case HAPI_AUDIO_SAMPLE_RATE_48_KHZ:
          m_params.audioSamplerate = 48000;
          break;
        case HAPI_AUDIO_SAMPLE_RATE_96_KHZ:
          m_params.audioSamplerate = 96000;
          break;
        case HAPI_AUDIO_SAMPLE_RATE_192_KHZ:
          m_params.audioSamplerate = 192000;
          break;
        case HAPI_AUDIO_SAMPLE_RATE_16_KHZ:
          m_params.audioSamplerate = 16000;
          break;
    }
#endif

    /*
      NOTE:
      These settings become "permanent and thus cannot be dynamically
      set later Don't set anything here that you want to change later
    */

    RegistryAccess::writeDword("VideoCapSource", m_params.videoInput);

#if 0 // Ignored
    RegistryAccess::writeDword("RateControl", m_params.videoRateControl);
#endif

    RegistryAccess::writeDword("VideoRateControl", m_params.videoRateControl);

    RegistryAccess::writeDword("VideoTSBitrate", m_params.tsBitrate);
    RegistryAccess::writeDword("VideoOutputBitrate", m_params.videoBitrate);
    RegistryAccess::writeDword("VBRMin", m_params.videoVBRMin);
    RegistryAccess::writeDword("VBRMax", m_params.videoVBRMax);

    RegistryAccess::writeDword("VideoProfile", m_params.videoProfile);
    RegistryAccess::writeDword("Profile", m_params.videoProfile);

#if 0
    RegistryAccess::writeDword("VideoCodecOutputFormat", 4);
    RegistryAccess::writeDword("Level", m_params.videoH264Level);
    RegistryAccess::writeDword("VideoLatency", m_params.videoLatency);
    RegistryAccess::writeDword("Latency", m_params.videoLatency);
#endif
    RegistryAccess::writeDword("VideoLevel", m_params.videoH264Level);

    RegistryAccess::writeDword("AudioCapSource", m_params.audioInput);
//    RegistryAccess::writeDword("AudioCapSPDIFInput", 3); // Default = 0

#if 0 // We may want to change these on the fly, so don't set them here!
    RegistryAccess::writeDword("AudioCodecOutputFormat", m_params.audioCodec);
    RegistryAccess::writeDword("AudioOutputSamplingRate",
                               m_params.audioSamplerate);
    RegistryAccess::writeDword("AudioCapSampleRate", m_params.audioSamplerate);

    RegistryAccess::writeDword("AudioOutputSamplingRate", 3);
    RegistryAccess::writeDword("AudioCapSampleRate", 3);

    RegistryAccess::writeDword("AudioOutputBitrate", m_params.audioBitrate);
#endif

//    RegistryAccess::writeDword("AudioCapMode", 5);

    // AudioOutputMode
    // -
}

bool HauppaugeDev::set_digital_audio(bool optical)
{
    try
    {
        // init CS8416 optical receiver if detected
        m_audio_CS8416 = new audio_CS8416(*m_fx2);
        if (optical)
            m_audio_CS8416->reset(audio_CS8416::AudioInput::OPTICAL);
        else
            m_audio_CS8416->reset(audio_CS8416::AudioInput::HDMI);

        INFOLOG << "CS8416 initialized.";
    }
    catch (std::runtime_error &e)
    {
        WARNLOG << "This device does not have a CS8416. "
                << "AC3 not available.";

        // Delete this so we know it doesn't exist
        delete m_audio_CS8416;
        m_audio_CS8416 = nullptr;
        return false;
    }

    return true;
}

bool HauppaugeDev::set_audio_format(encoderAudioInFormat_t audioFormat)
{
    bool spdif = (m_params.audioInput == HAPI_AUDIO_CAPTURE_SOURCE_SPDIF);

    if (spdif)
    {
        if (set_digital_audio(true))
        {
            m_fx2->setPortStateBits(FX2_PORT_E, 0x10, 0x00);
            DEBUGLOG << "Audio Input: S/PDIF";
        }
        else
        {
            WARNLOG << "Can't initialize CS8416 part. "
                    << "S/PDIF input is not available";
            spdif = false;
        }
    }

    if (!spdif)
    {
        switch (m_params.audioInput)
        {
            case HAPI_AUDIO_CAPTURE_SOURCE_HDMI:
            {
                if (set_digital_audio(false) && audioFormat == ENCAIF_AC3)
                {
                    m_fx2->setPortStateBits(FX2_PORT_E, 0x10, 0x00);
                    INFOLOG << "'SPDIF' from HDMI via 8416";
                }
                else
                {
#if 0               // Fred says this is wrong
                    m_fx2->setPortStateBits(FX2_PORT_E, 0, 0x18);
#else
                    m_fx2->setPortStateBits(FX2_PORT_E, 0x00, 0x00);
#endif
                    INFOLOG << "I2S audio from ADV7842";
                }
                INFOLOG << "Audio Input: HDMI";
                break;
            }
            case HAPI_AUDIO_CAPTURE_SOURCE_LR:
            default:
            {
                if (m_params.audioBoost)
                {
                    // Set audio input via RCA with audio boost
                    m_fx2->setPortStateBits(FX2_PORT_E, 0x18, 0);
                    INFOLOG << "Audio Input: RCA with boost";
                }
                else
                {
                    // Set audio input via RCA
                    m_fx2->setPortStateBits(FX2_PORT_E, 0x08, 0x10);
                    INFOLOG << "Audio Input: RCA";
                }

                break;
            }
        }
    }

    INFOLOG << "Audio codec: "
            << (audioFormat == ENCAIF_AC3 ? "AC3" : "AUTO");

    return true;
}

bool HauppaugeDev::set_input_format(encoderSource_t source,
                                    unsigned width, unsigned height,
                                    bool interlaced, float vFreq,
                                    float aspectRatio, float audioSampleRate)
{
    INFOLOG << "Input width: " << width
            << " height: " << height
            << " interlaced: " << interlaced
            << " vFreq: " << vFreq
            << " audio SR: " << audioSampleRate;

    encoderAudioInFormat_t audioFormat =
        (m_params.audioCodec == HAPI_AUDIO_CODEC_AC3 ? ENCAIF_AC3
                                                     : ENCAIF_AUTO);
    HAPI_AUDIO_CODEC audioCodec = m_params.audioCodec;

    set_audio_format(audioFormat);

    // Detect the current audio type if we didn't already specify AC3
    // Wait for up to 2 video frames to detect audio type
    if (audioFormat == ENCAIF_AUTO && m_audio_CS8416)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(66));
        uint8_t bppc0, format, state;
        for (int t = 66; t > 0; t -= 3)
        {
            m_audio_CS8416->direct_io()->read(
                audio_CS8416::DeviceIO::Reg::INTSTAT, (uint8_t *)&state,
                sizeof(state));
            m_audio_CS8416->direct_io()->read(
                audio_CS8416::DeviceIO::Reg::BRSTPREAPC0, (uint8_t *)&bppc0,
                sizeof(bppc0));
            m_audio_CS8416->direct_io()->read(
                audio_CS8416::DeviceIO::Reg::AFMTD, (uint8_t *)&format,
                sizeof(format));
            if (state & 1)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
        }

        audioCodec = getAudioCodec(format, bppc0);
        m_currentAudioCodec = audioCodec;
        encoderAudioInFormat_t newFormat =
            audioCodec == HAPI_AUDIO_CODEC_AAC ? ENCAIF_AUTO : ENCAIF_AC3;
        if (newFormat != audioFormat)
            set_audio_format(newFormat);
        audioFormat = newFormat;

        INFOLOG << "Detected audioCodec: " << audioCodec;
    }

    if (m_params.videoCodingMode >= HAPI_CODING_MODE_FIELD &&
        m_params.videoCodingMode <= HAPI_CODING_MODE_PAFF)
    {
        string desc;
        switch (m_params.videoCodingMode)
        {
            case HAPI_CODING_MODE_FRAME:
              desc = "Frame";
              break;
            case HAPI_CODING_MODE_FIELD:
              desc = "Field";

              break;
            case HAPI_CODING_MODE_MBAFF:
              desc = "MBAFF";
              break;
            case HAPI_CODING_MODE_PAFF:
              desc = "PAFF";
              break;
            default:
              desc = "Unknown";
        }

        if (interlaced)
        {
            INFOLOG << "Setting video coding mode to " << desc;
            RegistryAccess::writeDword("VideoCodingMode",
                                       m_params.videoCodingMode);
        }
        else
            INFOLOG << "Progressive video, "
                    << "ignoring requested video coding mode " << desc;
    }
    else
        DEBUGLOG << "Progressive video";

    if (interlaced && m_params.flipFields)
        FlipHDMIFields();

    if(!m_encDev->setInputFormat(source, audioFormat,
                                 width, height, interlaced,
                                 vFreq, aspectRatio,
                                 audioSampleRate, audioCodec))
    {
        CRITLOG << "Cannot set video mode";
        return false;
    }

    return true;
}

bool HauppaugeDev::valid_resolution(int width, int height)
{
    if (width == 0 || height == 0)
        return false;
    if (width != 1920 && width != 1280 && width != 720)
        return false;
    if (height != 1080 && height != 720 && height != 480 && height != 576)
        return false;
    return true;
}

bool HauppaugeDev::init_cvbs(void)
{
    if (m_video_initialized != HAPI_VIDEO_CAPTURE_SOURCE_CVBS)
    {
        m_rxDev->setInput(RXI_CVBS);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        m_rxDev->showInfo();
        m_encDev->showInfo();
    }

    int idx;
    for (idx = 0; idx < MAX_RETRY; ++idx)
    {
        if (m_rxDev->hasInputSignal())
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (idx == MAX_RETRY)
    {
        m_errmsg = "No input signal.";
        CRITLOG << m_errmsg;
        return false;
    }

    receiverOutputParams_t vp;
    for (idx = 0; idx < MAX_RETRY; ++idx)
    {
        // getOutputParams can return garbage
        if (m_rxDev->getOutputParams(&vp) &&
            valid_resolution(vp.width, vp.height))
            break;
        INFOLOG << "Invalid Output Params, retrying.";
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (idx == MAX_RETRY)
    {
        m_errmsg = "Cannot determine video mode.";
        CRITLOG << m_errmsg;
        return false;
    }

    float aspectRatio = vp.aspectRatio;

#if 0
    if (enforceAR != 0)
        aspectRatio = enforceAR;
#endif

    if (!set_input_format(ENCS_CVBS, vp.width, vp.height, vp.interlaced,
                          vp.vFreq, aspectRatio, 0))
        return false;

    m_rxDev->setOutputBusMode(RXOBM_656_10);

    INFOLOG << "Composite video input initialized.";

    m_video_initialized = HAPI_VIDEO_CAPTURE_SOURCE_CVBS;
    return true;
}

bool HauppaugeDev::init_component(void)
{
    if (m_video_initialized != HAPI_VIDEO_CAPTURE_SOURCE_COMPONENT)
    {
        m_rxDev->setInput(RXI_COMP);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        m_rxDev->showInfo();
        m_encDev->showInfo();
    }

    int idx;
    for (idx = 0; idx < MAX_RETRY; ++idx)
    {
        if (m_rxDev->hasInputSignal())
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (idx == MAX_RETRY)
    {
        m_errmsg = "No input signal.";
        CRITLOG << m_errmsg;
        return false;
    }

    receiverOutputParams_t vp;
    for (idx = 0; idx < MAX_RETRY; ++idx)
    {
        // getOutputParams can return garbage
        if (m_rxDev->getOutputParams(&vp) &&
            valid_resolution(vp.width, vp.height))
            break;
        INFOLOG << "Invalid Output Params (" << vp.width << "x" << vp.height
                << "), retrying.";
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (idx == MAX_RETRY)
    {
        m_errmsg = "Cannot determine video mode.";
        CRITLOG << m_errmsg;
        return false;
    }

    float aspectRatio = vp.aspectRatio;
#if 0
    if (enforceAR != 0)
        aspectRatio = enforceAR;
#endif

    if (!set_input_format(ENCS_COMP, vp.width, vp.height, vp.interlaced,
                          vp.vFreq, aspectRatio, 0))
        return false;

    if(((vp.height == 480) && (roundf(vp.vFreq) == 60) && vp.interlaced) ||
       ((vp.height == 576) && (roundf(vp.vFreq) == 50) && vp.interlaced))
        m_rxDev->setOutputBusMode(RXOBM_656_10_DC);
    else
        m_rxDev->setOutputBusMode(RXOBM_422_10x2);

    audio_CX2081x audio_CX2081x(*m_fx2);
    audio_CX2081x.init();

    INFOLOG << "Component video input initialized.";

    m_video_initialized = HAPI_VIDEO_CAPTURE_SOURCE_COMPONENT;
    return true;
}

bool HauppaugeDev::init_sdi(void)
{
    m_errmsg = "SDI not supported.";
    CRITLOG << m_errmsg;
    return false;
}

bool HauppaugeDev::init_hdmi(void)
{
    if (m_video_initialized != HAPI_VIDEO_CAPTURE_SOURCE_HDMI)
    {
        m_rxDev->setInput(RXI_HDMI);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        m_rxDev->showInfo();
        m_encDev->showInfo();
    }

    int idx;
    for (idx = 0; idx < MAX_RETRY; ++idx)
    {
        if (m_rxDev->hasInputSignal())
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (idx == MAX_RETRY)
    {
        m_errmsg = "No input signal.";
        CRITLOG << m_errmsg;
        return false;
    }

    receiverAudioParams_t ap;
    ap.sampleRate = 0; // default
    m_rxDev->getAudioParams(&ap);
    ap.sampleRate = 48000; // default

    int vic = m_rxDev->getHDMIFormat();
    if (vic <= 0 || !m_encDev->setHDMIFormat(vic, ap.sampleRate))
    {
        receiverHDMIParams_t vp;
        for (idx = 0; idx < MAX_RETRY; ++idx)
        {
            // getHDMIParams can return garbage
            if (m_rxDev->getHDMIParams(&vp) &&
                valid_resolution(vp.width, vp.height))
                break;
            INFOLOG << "Invalid Output Params, retrying.";
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (idx == MAX_RETRY)
        {
            m_errmsg = "Cannot determine video mode.";
            CRITLOG << m_errmsg;
            return false;
        }

        if (!set_input_format(ENCS_HDMI, vp.width, vp.height,
                              vp.interlaced, vp.vFreq,
                              16.0f/9, ap.sampleRate))
            return false;

        // there should not be any such modes in DMT... Or maybe
        // bad InfoFrame?
        m_rxDev->setOutputBusMode(RXOBM_422_10x2);
    }
    else
    {
        switch (vic)
        { // TODO: will be moved into device_t class soon
            case 6:
            case 7:
            case 21:
            case 22:
              m_rxDev->setOutputBusMode(RXOBM_656_10_DC);
              break;
            default:
              m_rxDev->setOutputBusMode(RXOBM_422_10x2);
        }
    }

#if 0
    if (enforceAR != 0)
        m_encDev->setHDMIAR(enforceAR);
#endif

    INFOLOG << "HDMI video input initialized.";

    m_video_initialized = HAPI_VIDEO_CAPTURE_SOURCE_HDMI;
    return true;
}

bool HauppaugeDev::open_file(const string & file_name)
{
    if (file_name == "stdout")
        m_fd = 1;
    else
        m_fd = open(file_name.c_str(), O_WRONLY | O_TRUNC | O_CREAT, 0666);

    if (m_fd < 0)
    {
        m_errmsg = "Failed to open '" + file_name + "' :" + strerror(errno);
        ERRORLOG << m_errmsg;
        return false;
    }
    INFOLOG << "Output to '" << file_name << "'";
    return true;
}

void HauppaugeDev::log_ports(void)
{
    DEBUGLOG << hex
             << "\nPORT_A: [0x" << m_fx2->getPortDir(FX2_PORT_A)
             << "] 0x" << m_fx2->getPortState(FX2_PORT_A) << "\n\t"
             << "PORT_B: [0x" <<  m_fx2->getPortDir(FX2_PORT_B) << "] "
             << "0x" << m_fx2->getPortState(FX2_PORT_B) << "\n\t"
             << "PORT_C: [0x" << m_fx2->getPortDir(FX2_PORT_C) << "] "
             << "0x" << m_fx2->getPortState(FX2_PORT_C) << "\n\t"
             << "PORT_D: [0x" << m_fx2->getPortDir(FX2_PORT_D) << "] "
             << "0x" << m_fx2->getPortState(FX2_PORT_D) << "\n\t"
             << "PORT_E: [0x" << m_fx2->getPortDir(FX2_PORT_E) << "] "
             << "0x" << m_fx2->getPortState(FX2_PORT_E) << "\n\t"
             << "CTL_PORTS: [0x" << m_fx2->getPortDir(FX2_CTL_PORTS)
             << "] 0x" << m_fx2->getPortState(FX2_CTL_PORTS)
             << dec;
}

bool HauppaugeDev::Open(USBWrapper_t & usbio, bool ac3,
                        DataTransfer::callback_t * cb)
{
    INFOLOG << "Opening Hauppauge USB device.";

    if (m_params.verbose)
    {
        string desc;
        usbio.USBDevDesc(desc);
        DEBUGLOG << desc;
    }

    m_fx2 = new FX2Device_t(usbio);

    int ret;
    int idx =0;
    for ( ; !m_fx2->isUSBHighSpeed() && idx < MAX_RETRY; ++idx)
    {
        INFOLOG << "Reloading firmware, attempt " << (idx + 1);
        m_fx2->stopCPU();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        m_fx2->loadFirmware();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        m_fx2->startCPU();
        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        do {
            usbio.Close();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            ret = usbio.Open(m_params.serial);
        } while (ret < 0);
    }

    if (idx == MAX_RETRY)
    {
        CRITLOG << "Could not communicate with USB device";
        return false;
    }

    INFOLOG << "FX2 ready after " << idx << " tries.";
    log_ports();

    // reset CS5340, it will be set back by m_encDev->init()
    if (!m_fx2->setPortStateBits(FX2_CTL_PORTS, 0, 0x10))
    {
        CRITLOG << "Error setting FX2_CTL_PORT";
        return false;
    }

    m_encDev = new encoderDev_DXT_t(*m_fx2);
    if (!m_encDev->init())
    {
        m_errmsg = "Encoder init failed.";
        CRITLOG << m_errmsg;
        return false;
    }

    DEBUGLOG << hex
             << "\nPORT_A: 0x" << m_fx2->getPortState(FX2_PORT_A)
             << "\nPORT_E: 0x" << m_fx2->getPortState(FX2_PORT_E)
             << dec;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    INFOLOG << "encDev ready";

    m_rxDev = new receiver_ADV7842_t(*m_fx2);
    if (ac3)
    {
#if 1
        m_rxDev->setEDID(edidHDPVR2_1080p6050_ac3_fix_rgb,
                         sizeof(edidHDPVR2_1080p6050_ac3_fix_rgb),
                         edidHDPVR2_1080p6050_ac3_fix_rgbSpaLoc);
        INFOLOG << "Using 1080p6050 w/AC3 EDID.";
#else
        m_rxDev->setEDID(edidHDPVR2_1080p6050_atmos,
                         sizeof(edidHDPVR2_1080p6050_ac3_fix_rgb),
                         edidHDPVR2_1080p6050_atmos_SPAloc);
        INFOLOG << "Using 1080p6050 w/Atmos EDID.";
#endif
    }
    else
    {
#if 0 // Original EDID
        m_rxDev->setEDID(EDID_default, sizeof(EDID_default),
                         EDID_default_SPAloc);
        INFOLOG << "Using 1080p6050 stereo EDID.";
#else // Updated EDID from Hauppauge
        m_rxDev->setEDID(edidHDPVR2_1080p6050_pcm_fix_rgb,
                         sizeof(edidHDPVR2_1080p6050_pcm_fix_rgb),
                         edidHDPVR2_1080p6050_pcm_fix_rgbSpaLoc);
        INFOLOG << "Using 1080p6050 stereo RGB EDID.";
#endif
    }
    m_rxDev->init();
    INFOLOG << "rxDev ready";

    if (!m_params.output.empty())
    {
        if (!open_file(m_params.output))
            return false;
        m_encDev->setOutputFD(m_fd);
    }
    if (cb)
        m_encDev->setWriteCallback(*cb);

    INFOLOG << "Hauppauge USB device ready.";

    return true;
}

void HauppaugeDev::Close(void)
{
    if (m_rxDev)
    {
        delete m_rxDev;
        m_rxDev = nullptr;
    }
    if (m_encDev)
    {
        delete m_encDev;
        m_encDev = nullptr;
    }
    if (m_fx2)
    {
        delete m_fx2;
        m_fx2 = nullptr;
    }

    delete m_audio_CS8416;
    m_audio_CS8416 = nullptr;
}

bool HauppaugeDev::StartEncoding(void)
{
    switch (m_params.videoInput)
    {
        case HAPI_VIDEO_CAPTURE_SOURCE_CVBS:
          if (!init_cvbs())
              return false;
          break;
        case HAPI_VIDEO_CAPTURE_SOURCE_COMPONENT:
          if (!init_component())
              return false;
          break;
        case HAPI_VIDEO_CAPTURE_SOURCE_SDI:
          if (!init_sdi())
              return false;
          break;
        case HAPI_VIDEO_CAPTURE_SOURCE_HDMI:
        default:
          if (!init_hdmi())
              return false;
          break;
    }

    if (!m_encDev->startCapture())
    {
        m_errmsg = "Encoder start capture failed.";
        ERRORLOG << m_errmsg;
        return false;
    }
    log_ports();

    if (m_audio_CS8416)
    {
        m_audioMonitorThread =
            std::thread(&HauppaugeDev::audioMonitorLoop, this);
    }

    NOTICELOG << "Capture started.";
    return true;
}

bool HauppaugeDev::StopEncoding(void)
{
    // Stop the thread first in case its busy switching  audioCodecs
    if (m_audioMonitorThread.joinable())
    {
        m_exitAudioMonitorLoop = true;
        m_audioMonitorThread.join();
    }

    if (!m_encDev->stopCapture())
    {
        m_errmsg = "Encoder stop capture failed.";
        WARNLOG << m_errmsg;
        return false;
    }
    log_ports();

    return true;
}

static std::string stateToBits(uint8_t state)
{
    std::ostringstream out;

    for (int i = 0; i < 8; ++i)
    {
        out << ((state & (1 << (7 - i))) != 0 ? "1" : "0");
    }
    out << setw(5) << (int)state;

    return out.str();
}

void HauppaugeDev::audioMonitorLoop(void)
{
    uint8_t bppc0, bppc1, bppd0, bppd1;
    uint8_t state;
    uint8_t format;
    std::chrono::high_resolution_clock::time_point t1 =
        std::chrono::high_resolution_clock::now();

    m_exitAudioMonitorLoop = false;

    setThreadName("audioMonitor");
    NOTICELOG << "audioMonitorThread starting";

    while (!m_exitAudioMonitorLoop)
    {
        // Poll for audio format changes.
        // Does this belong in a separate thread?
        m_audio_CS8416->direct_io()->read(audio_CS8416::DeviceIO::Reg::INTSTAT,
                                        (uint8_t *)&state, sizeof(state));
        m_audio_CS8416->direct_io()->read(audio_CS8416::DeviceIO::Reg::BRSTPREAPC0,
                                        (uint8_t *)&bppc0, sizeof(bppc0));
        m_audio_CS8416->direct_io()->read(audio_CS8416::DeviceIO::Reg::AFMTD,
                                        (uint8_t *)&format, sizeof(format));

        // state & 1 means a true format change. !(format & 1) with
        // bppc0 == 21 means DD+ went away

        if (std::chrono::high_resolution_clock::now() > t1 && ((state & 1) || ((format & 1) == 0 && bppc0 == 21)))
        {
            HAPI_AUDIO_CODEC audioCodec = getAudioCodec(format, bppc0);

            if (audioCodec != m_currentAudioCodec)
            {
                m_audio_CS8416->direct_io()->read(audio_CS8416::DeviceIO::Reg::BRSTPREAPC1,
                                                (uint8_t *)&bppc1, sizeof(bppc1));
                m_audio_CS8416->direct_io()->read(audio_CS8416::DeviceIO::Reg::BRSTPREAPD0,
                                                (uint8_t *)&bppd0, sizeof(bppd0));
                m_audio_CS8416->direct_io()->read(audio_CS8416::DeviceIO::Reg::BRSTPREAPD1,
                                                (uint8_t *)&bppd1, sizeof(bppd1));
                NOTICELOG << "New audio state: " << stateToBits(state)
                                << " format: " << stateToBits(format)
                                << " bppc0: " << std::setw(4) << (int)bppc0
                                << " bppc1: " << std::setw(4) << (int)bppc1
                                << " bppd0: " << std::setw(4) << (int)bppd0
                                << " bppd1: " << std::setw(4) << (int)bppd1;

                switch (audioCodec)
                {
                    case HAPI_AUDIO_CODEC_AC3:
                    NOTICELOG << ((int)bppc0 == 21 ?
                                            "Detected: Dolby Digital Plus  " :
                                            "Detected: AC-3  ");
                    break;
                    case HAPI_AUDIO_CODEC_MPEG:
                    NOTICELOG << "Detected: MPEG";
                    break;
                    case HAPI_AUDIO_CODEC_MP3:
                    NOTICELOG << "Detected: MP3";
                    break;
                    case HAPI_AUDIO_ENCODING_MPEG2:
                    NOTICELOG << "Detected: MPEG2";
                    break;
                    case HAPI_AUDIO_CODEC_AAC:
                    NOTICELOG << "Detected: AAC/PCM";
                    break;
                    case HAPI_AUDIO_ENCODING_DTS:
                    NOTICELOG << "Detected: DTS";
                    break;
                    default:
                    NOTICELOG << "Detected: Unknown";
                    break;
                }

                setAudioMode(audioCodec);

                m_currentAudioCodec = audioCodec;
                t1 = std::chrono::high_resolution_clock::now()
                     + std::chrono::milliseconds(2000);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    NOTICELOG << "audioMonitorThread exiting";
    // NOTICELOG << "Audio state: " << stateToBits(state)
    //                    << " format: " << stateToBits(format)
    //                    << " bppc0: " << setw(4) << (int)bppc0
    //                    << " bppc1: " << setw(4) << (int)bppc1
    //                    << " bppd0: " << setw(4) << (int)bppd0
    //                    << " bppd1: " << setw(4) << (int)bppd1;
}

bool HauppaugeDev::setAudioMode(HAPI_AUDIO_CODEC audioCodec)
{
    NOTICELOG << "Setting new audio mode";
    bool r = m_encDev->setAudioMode(audioCodec);
    NOTICELOG << "Setting new audio mode done";
    return r;
}

HAPI_AUDIO_CODEC HauppaugeDev::getAudioCodec(uint8_t format, uint8_t bppc0)
{
    /*
    format is from the AFMTD register of the CS8416
    bppc0 is the PC0 byte of the burst preamble from the CS8416

    Normally we could rely on bit 5 of the format to be set for some
    type of encoded audio but DD+ isn't detected correctly. Right now,
    the only unique thing about it is that bit 5 is clear, bit 6
    (PCM data) fluctuates, but bit 1 is always set. So we AND
    the format with 33 to check for normal encoded (32) and
    >=88.1k sample rate (1).

    To detect the other formats, we use the burst preamble byte.

    From: http://www.farnell.com/datasheets/1360564.pdf

    PC[4:0], Hex Data Type
    00 Null
    01 Dolby AC-3
    02 Reserved
    03 Pause
    04 MPEG-1 layer 1
    05 MPEG-1 layer 2 or 3, or MPEG-2 without extension
    06 MPEG-2 Data with extension
    07 MPEG-2 AAC ADTS
    08 MPEG-2 layer 1 low sample rate
    09 MPEG-2 layer 2 or 3 low sample rate
    0A Reserved
    0B DTS type 1
    0C DTS type 2
    0D DTS type 3
    0E ATRAC
    0F ATRAC2/3
    10-1F Reserved
    */

    //NOTICELOG << "Getting audioCodec from format/bppc0: " << (int)format << " " << (int)bppc0;

    HAPI_AUDIO_CODEC audioCodec;

    if (format & 33)
    { // This checks whether its encoded audio or the weird case of DD+
        switch (bppc0 & 0x1f)
        {
            case 1:
            audioCodec = HAPI_AUDIO_CODEC_AC3;
            break;
            case 4:
            audioCodec = HAPI_AUDIO_CODEC_MPEG;
            break;
            case 5:
            audioCodec = HAPI_AUDIO_CODEC_MP3;
            break; // ??
            case 6:
            audioCodec = HAPI_AUDIO_ENCODING_MPEG2;
            break; // ??
            case 7:
            audioCodec = HAPI_AUDIO_CODEC_AAC;
            break; // ??
            case 11:
            case 12:
            case 13:
            audioCodec = HAPI_AUDIO_ENCODING_DTS;
            break;
            case 14:
            case 15:
            audioCodec = HAPI_AUDIO_CODEC_AAC /* ??? */;
            break;
            case 21:
            audioCodec =
                format & 1 ? HAPI_AUDIO_CODEC_AC3 : HAPI_AUDIO_CODEC_AAC;
            break;
            default:
            audioCodec = HAPI_AUDIO_CODEC_AAC;
            break;
        }
    }
    else
    {
        audioCodec = HAPI_AUDIO_CODEC_AAC;
    }
    return audioCodec;
}
