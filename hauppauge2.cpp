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

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/tokenizer.hpp>
#include <boost/token_functions.hpp>

#include "Logger.h"
#include "Common.h"
#include "HauppaugeDev.h"
#include "MythTV.h"

#include <iostream>
#include <fstream>

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "types_local.h"

namespace po = boost::program_options;

void InitInterruptHandler(void)
{
    sigset_t ss;
    sigemptyset(&ss);
    sigaddset(&ss, SIGINT);
    pthread_sigmask(SIG_BLOCK, &ss, NULL);
}

static int evtWait() {
        sigset_t ss;
        sigemptyset(&ss);
        sigaddset(&ss, SIGINT);
        int s;
        if(sigwait(&ss, &s) != 0) return 0;
        return s;
}

bool ListDevs(void)
{
    USBWrapper_t usbio;
    DeviceIDVec devs;
    DeviceIDVec::iterator Idev;

    if (!usbio.DeviceList(devs))
    {
        cerr << usbio.ErrorString();
        return false;
    }

    for (Idev = devs.begin(); Idev != devs.end(); ++Idev)
    {
        cout << "[Bus: " << static_cast<int>(get<4>(*Idev))
             << " Port: " << static_cast<int>(get<5>(*Idev))
             << "]  " << hex << get<0>(*Idev)
             << ":0x" << get<1>(*Idev)
             << " " << get<2>(*Idev) << " " << get<3>(*Idev)
             << endl;

        if (!usbio.Open(get<2>(*Idev)))
        {
            cerr << usbio.ErrorString();
            return false;
        }

        string desc;
        usbio.USBDevDesc(desc);
        cout << desc;
        cout << endl;

        usbio.Close();
    }

    return true;
}

bool FindDev(const string & serial, int & bus, int & port)
{
    USBWrapper_t usbio;
    DeviceIDVec devs;
    DeviceIDVec::iterator Idev;

    if (!usbio.DeviceList(devs))
    {
        cerr << usbio.ErrorString();
        return false;
    }

    for (Idev = devs.begin(); Idev != devs.end(); ++Idev)
    {
        if (get<2>(*Idev) == serial)
        {
            bus = static_cast<int>(get<4>(*Idev));
            port = static_cast<int>(get<5>(*Idev));
            return true;
        }
    }
    return false;
}

int main(int argc, char *argv[])
{
    Parameters params;
    int bus, port;

    po::options_description cmd_line("Hauppauge libusb based device recorder");
    cmd_line.add_options()
        ("help,h", "Produce this help message.")
        ("config,c", po::value<std::string>(), "Config file")
        ("list,l", "List detected devices.");

    po::options_description config{"Configuration options"};
    config.add_options()
        ("verbose,v", po::value<int>()->implicit_value(1),
         "Verbose state information.")
        ("input,i", po::value<int>()->default_value(3),
         "Video input (0=COMPOSITE, 1=COMPONENT, 2=SDI, 3=HDMI)")
        ("audio,a", po::value<int>()->default_value(3),
         "Audio input (0=RCA, 1=SPDIF, 2=SDI, 3=HDMI)")
        ("codec,d", po::value<int>()->default_value(3),  // 2?
         "Audio codec (1=MPEG, 2=AC3, 3=AAC, 6=MP3, 8=PCM, 9=PASSTHROUGH)")
#if 0
        ("audiomode,e", po::value<int>()->default_value(3),
         "Audio capture mode (0=None, 1=Frame, 2=frame-frame, "
         "3=AAC(Sub_frame), 5=Downmix")
#endif
        ("boost,b", "Boost audio input gain.")
        ("audiosamplerate,S", po::value<int>()->default_value(3),
         "Analog audio sample rate (-1=FOLLOW_INPUT, 0=NONE, 1=32KHz, "
         "2=44.1KHz, 3=48KHz, 4=96KHz, 5=192KHz, 6=16KHz)")
        ("audiobitrate,R", po::value<int>()->default_value(29),
         "Analog audio bitrate (-1=NONE, 0=32Kbps, 1=48Kbps, 2=56Kbps, "
         "3=64Kbps, 4=80Kbps, 5=96Kbps, 6=112Kbps, 7=128Kbps, 8=160Kbps, "
         "9=192Kbps, 10=224Kbps, 11=256Kbps, 12=320Kbps, 13=384Kbps, "
         "14=448Kbps, 15=576Kbps, 16=16Kbps, 17=8Kbps, 18=640Kbps, "
         "19=768Kbps, 20=960Kbps, 21=1024Kbps, 22=1152Kbps, 23=1280Kbps, "
         "24=1344Kbps, 25=1408Kbps, 26=1411Kbps, 27=1472Kbps, 28=1536Kbps, "
         "29=MAX)")
        ("videobitrate,r", po::value<int>()->default_value(8000000),
         "Video bitrate")
        ("videoratecontrol,C", po::value<int>()->default_value(1),
         "Video rate type (0=CBR, 1=VBR, 2=CAPPED_VBR)")
        ("minvbrrate,m", po::value<int>()->default_value(9000000),
         "Mininum VBR bitrate")
        ("maxvbrrate,M", po::value<int>()->default_value(20000000),
         "Maximum VBR bitrate")
        ("tsbitrate,t", po::value<int>()->default_value(20000000),
         "Transport Stream bitrate")
        ("profile,p", po::value<int>()->default_value(4), "H.264 profile "
         "(0=Constrained baseline, 3=Main, 4=High)")
        ("level,L", po::value<int>()->default_value(420),
         "H.264 Level (100=LEVEL_1, 105=LEVEL_1B,"
         "110=LEVEL_1.1, 120=LEVEL_1.2, 130=LEVEL_1.3, 200=LEVEL_2, "
         "210=LEVEL_2.1, 220=LEVEL_2.2, 300=LEVEL_3, 310=LEVEL_3.1, "
         "320=LEVEL_3.2, 400=LEVEL_4, 410=LEVEL_4.1, 420=LEVEL_4.2, "
         "500=LEVEL_5, 510=LEVEL_5.1)")
        ("bframes,B", po::value<int>()->default_value(3),
         "Number of B-Frames (0-7)")
        ("videoLatency,A", po::value<int>()->default_value(1),
         "Video latency (0=low latency, 1=high latency)")
        ("aspect", po::value<float>()->default_value(1.0), "Aspect ratio")
        ("serial,s", po::value<string>(), "Serial number of device.")
        ("output,o", po::value<string>(), "Output destination")
        ("mythtv", po::value<bool>()->implicit_value(true),
         "Operate in MythTV External Recorder mode")

        // Logging
        ("logpath", po::value<string>(),
         "Writes logging messages to a file in the directory "
         "logpath with filenames in the format: hauppauge2-<serial#>.pid.log")
        ("loglevel", po::value<string>()->default_value("notice"),
         "Set the logging level.  All log messages at lower "
         "levels will be discarded. In descending order: emerg, alert, crit, "
         "err, warning, notice, info, debug")
        ("quiet,q", po::value<int>()->implicit_value(1),
         "Don't log to the console (-q).");

    po::options_description config_file{"Config file options"};
    config_file.add_options()
        ("override-loglevel", po::value<string>()->default_value("notice"),
         "Overrides the command-line logging level.  All log messages at lower "
         "levels will be discarded. In descending order: emerg, alert, crit, "
         "err, warning, notice, info, debug");

    Logger::setThreadName("main");

    po::options_description cmdline_options;
    cmdline_options.add(cmd_line).add(config);

    po::options_description config_file_options;
    config_file_options.add(config).add(config_file);

    po::variables_map vm;


    // Parse the command line catching and displaying any parser
    // errors
    try
    {
        store(po::command_line_parser(argc, argv)
              .options(cmdline_options).run(), vm);
    }
    catch (std::exception &e)
    {
        cout << endl << e.what() << endl;
        cout << cmdline_options << endl;
    }

    try
    {
        if (vm.count("config"))
        {
            std::ifstream ifs{vm["config"].as<std::string>().c_str()};
            if (ifs)
                store(parse_config_file(ifs, config_file_options), vm);
        }
    }
    catch (std::exception &e)
    {
        cout << endl << e.what() << endl;
        cout << config_file_options << endl;
    }

    notify(vm);

    InitInterruptHandler();

    // Display help text when requested
    if (vm.count("help"))
    {
        cout << cmdline_options << endl;
        return 0;
    }

    params.verbose = vm.count("verbose");

    if (vm.count("list"))
    {
        ListDevs();
        return 0;
    }

//    Logger::Get()->suppressLoc(true);
    if (vm.count("quiet"))
        Logger::Get()->setQuiet(true);

    if (vm.count("override-loglevel"))
        Logger::Get()->setFilter(vm["override-loglevel"].as<string>());
    else if (vm.count("loglevel"))
        Logger::Get()->setFilter(vm["loglevel"].as<string>());

    if (vm.count("logpath") && vm.count("quiet") < 2)
    {
        ostringstream path;
        path << vm["logpath"].as<string>() << "/" << "hauppauge2";
        if (vm.count("serial"))
            path << "-" << vm["serial"].as<string>();
        path << ".log";
        if (!Logger::Get()->setFile(path.str()))
            return -1;
    }

    if (vm.count("serial"))
    {
        params.serial = vm["serial"].as<string>();

        if (!FindDev(params.serial, bus, port))
        {
            LOG(Logger::CRIT) << "Device " << params.serial
                              << " not found on USB bus.\n" << flush;
            ListDevs();
            return -1;
        }
        LOG(Logger::CRIT) << "Initializing [Bus: " << bus
                          << ", Port: " << port << "] " << params.serial
                          << flush;
    }

    if (vm.count("input"))
        params.videoInput = static_cast<_HAPI_VIDEO_CAPTURE_SOURCE>
                             (vm["input"].as<int>());
    if (vm.count("audio"))
        params.audioInput = static_cast<_HAPI_AUDIO_CAPTURE_SOURCE>
                             (vm["audio"].as<int>());
    if (vm.count("codec"))
        params.audioCodec = static_cast<HAPI_AUDIO_CODEC>
                       (vm["codec"].as<int>());
    params.audioBoost = (vm.count("boost") != 0);
    if (vm.count("audiosamplerate"))
        params.audioSamplerate = static_cast<HAPI_AUDIO_SAMPLE_RATE>
                                   (vm["audiosamplerate"].as<int>());
    if (vm.count("audiobitrate"))
        params.audioBitrate = static_cast<_HAPI_AUDIO_BITRATE>
                               (vm["audiobitrate"].as<int>());
    if (vm.count("videobitrate"))
        params.videoBitrate = vm["videobitrate"].as<int>();
    if (vm.count("tsbitrate"))
        params.tsBitrate = vm["tsbitrate"].as<int>();
    if (vm.count("videoratecontrol"))
        params.videoRateControl = static_cast<_HAPI_RATE_CONTROL>
                                    (vm["videoratecontrol"].as<int>());
    if (vm.count("minvbrrate"))
        params.videoVBRMin = vm["minvbrrate"].as<int>();
    if (vm.count("maxvbrrate"))
        params.videoVBRMax = vm["maxvbrrate"].as<int>();
    if (vm.count("profile"))
        params.videoProfile = static_cast<_HAPI_VIDEO_PROFILE>
                               (vm["profile"].as<int>());
    if (vm.count("level"))
        params.videoH264Level = static_cast<_HAPI_VIDEO_H264_LEVEL>
                            (vm["level"].as<int>());
    if (vm.count("bframes"))
    {
        params.bFrames = vm["bframes"].as<int>();
        if (params.bFrames > 7) params.bFrames = 7;
    }
    if (vm.count("videolatency"))
        params.videoLatency = static_cast<_HAPI_LATENCY>
                         (vm["videolatency"].as<int>());
    if (vm.count("aspect"))
        params.aspectRatio = vm["aspect"].as<float>();

    params.mythtv = (vm.count("mythtv"));

    if (vm.count("output") || params.mythtv)
    {
        if (params.mythtv)
        {
            MythTV mythtv(params);
            mythtv.Wait();
        }
        else
        {
            params.output = vm["output"].as<string>();

            HauppaugeDev dev(params);
            if (!dev)
                return -2;

            USBWrapper_t usbio;
            if (!usbio.Open(params.serial))
                return -3;

            if (!dev.Open(usbio))
            {
                usbio.Close();
                return -4;
            }

            if (dev.StartEncoding())
            {
                evtWait();
                dev.StopEncoding();
            }

            dev.Close();
        }
    }

    LOG(Logger::CRIT) << "Done." << flush;
    return 0;
}