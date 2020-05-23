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

#include "MythTV.h"
#include "Logger.h"
#include <unistd.h>
#include <poll.h>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string.hpp>

#include "Transcoder.h"

using namespace std;
using namespace boost::algorithm;

MythTV::MythTV(const Parameters & params, const string & desc)
    : m_desc(desc)
    , m_buffer_max(188 * 100000)
    , m_block_size(m_buffer_max / 4)
    , m_buffer(this)
    , m_commands(this)
    , m_params(params)
    , m_dev(nullptr)
    , m_run(true)
    , m_fatal(false)
    , m_streaming(false)
    , m_xon(false)
    , m_ready(false)
    , m_error_cb(std::bind(&MythTV::USBError, this))
{
    m_buffer.Start();
    m_commands.Start();
}

MythTV::~MythTV(void)
{
    Terminate();

    m_commands.Join();
    m_buffer.Join();

    m_flow_mutex.lock();
    delete m_dev;
    m_dev = nullptr;
    m_flow_mutex.unlock();
}

void MythTV::Terminate(void)
{
    CRITLOG << "Terminating.";
    m_run = false;

    string msg;
    StopEncoding(msg, true);

    m_flow_cond.notify_all();
    m_run_cond.notify_all();
}

void MythTV::Wait(void)
{
    for (;;)
    {
        {
            std::unique_lock<std::mutex> lk(m_run_mutex);

            m_run_cond.wait_for(lk, std::chrono::milliseconds(2500));
            if (!m_run)
                break;
        }

        if (m_streaming)
        {
            // Check for wedged state.
            auto tm = std::chrono::system_clock::now() -
                      std::chrono::seconds(60);
            if (m_buffer.HeartBeat() < tm)
                Fatal("We seem to be wedged!");
        }
    }
}

void MythTV::OpenDev(void)
{
    std::lock_guard<std::timed_mutex> lock(m_flow_mutex);

    if (m_dev != nullptr)
        return;

    m_dev = new HauppaugeDev(m_params);
    if (!m_dev)
    {
        Fatal("Unable to create Hauppauge dev.");
        return;
    }
    if (!(*m_dev))
    {
        delete m_dev;
        m_dev = nullptr;
        Fatal(m_dev->ErrorString());
        return;
    }

    if (!m_usbio.Open(m_params.serial, &getErrorCallBack()))
    {
        delete m_dev;
        m_dev = nullptr;
        Fatal(m_usbio.ErrorString());
        return;
    }

    Transcoder transcoder(std::string(), &getWriteCallBack());
    if (!m_dev->Open(m_usbio, (m_params.audioCodec == HAPI_AUDIO_CODEC_AC3),
                     transcoder))
    {
        Fatal(m_dev->ErrorString());
        delete m_dev;
        m_dev = nullptr;

        m_usbio.Close();

        return;
    }

    m_ready = true;
    m_flow_cond.notify_all();
}

void MythTV::Fatal(const string & msg)
{
    CRITLOG << msg;

    std::unique_lock<std::mutex> lk(m_run_mutex);
    if (!m_fatal)
    {
        m_fatal_msg = msg;
        m_fatal = true;

        Terminate();
    }
}

bool MythTV::StartEncoding(string & resultmsg)
{
    if (m_streaming)
    {
        resultmsg = "Already streaming!";
        WARNLOG << resultmsg;
        return true;
    }
    if (!m_ready)
    {
        resultmsg = "Hauppauge device not ready.";
        CRITLOG << resultmsg;
        return false;
    }

    if (!m_dev->StartEncoding())
    {
        resultmsg = m_dev->ErrorString();
        return false;
    }

    resultmsg.clear();
    m_streaming = true;
    m_flow_cond.notify_all();
    return true;
}

bool MythTV::StopEncoding(string & resultmsg, bool soft)
{
    if (!m_streaming)
    {
        if (!soft)
        {
            resultmsg = "Already not streaming!";
            WARNLOG << resultmsg;
        }
        return true;
    }
    if (!m_ready)
    {
        resultmsg = "Hauppauge device not ready.";
        CRITLOG << resultmsg;
        return false;
    }

    m_flow_mutex.lock();
    if (!m_dev)
    {
        resultmsg = "Invalid Hauppauge device.";
        CRITLOG << resultmsg;
        m_flow_mutex.unlock();
        return false;
    }
    m_flow_mutex.unlock();

    m_streaming = false;
    m_flow_cond.notify_all();

    INFOLOG << "Stopping encoder.";
    if (!m_dev->StopEncoding())
    {
        resultmsg = m_dev->ErrorString();
        return false;
    }

    resultmsg.clear();
    INFOLOG << "Encoder stopped";
    return true;
}

Commands::Commands(MythTV * parent)
    : m_thread()
    , m_parent(parent)
    , m_api_version(1)
{
}

bool Commands::send_status(const string & command, const string & serial,
                           const string & status)
{
    string buf;
    if (m_api_version > 1)
        buf = serial + ":" + status + "\n";
    else
        buf = status + "\n";

    int len = write(2, buf.data(), buf.size());

    if (len != static_cast<int>(buf.size()))
    {
        ERRORLOG << "Status -- Wrote " << len
                 << " of " << buf.size()
                 << " bytes of status '" << status << "'";
        return false;
    }
    else
        INFOLOG << command << ": '"
                << serial << ":" << status << "'";

    return true;
}

bool Commands::process_command(const string & cmd)
{
    DEBUGLOG << "Processing '" << cmd << "'";

    if (starts_with(cmd, "APIVersion?"))
    {
        std::unique_lock<std::mutex> lk(m_parent->m_run_mutex);
        if (m_parent->m_fatal)
            send_status(cmd, "", "ERR:" + m_parent->m_fatal_msg);
        else
        {
            send_status(cmd, "", "OK:" + std::to_string(max_api_version));
            m_api_version = max_api_version;
        }
        return true;
    }

    string         serial;
    deque<string>  tokens;
    boost::split(tokens, cmd, boost::is_any_of(":"));
    if (m_api_version > 1)
    {
        if (tokens.size() < 2)
        {
            send_status(cmd, "-1", "ERR:Malformed message. Expecting "
                        "<serial>:command<:optional data>, but received \"" +
                        cmd + "\"");
            return true;
        }
        serial = tokens[0];
        tokens.pop_front();
    }

    if (starts_with(tokens[0], "Version?"))
    {
        std::unique_lock<std::mutex> lk(m_parent->m_run_mutex);
        if (m_parent->m_fatal)
            send_status(cmd, serial, "ERR:" + m_parent->m_fatal_msg);
        else
            send_status(cmd, serial, "OK:" + m_parent->m_params.version);
        return true;
    }
    if (starts_with(tokens[0], "Description?"))
    {
        send_status(cmd, serial, "OK:" + m_parent->m_desc);
        return true;
    }
    if (starts_with(tokens[0], "HasLock?"))
    {
        if (m_parent->m_ready)
            send_status(cmd, serial, "OK:Yes");
        else
            send_status(cmd, serial, "OK:No");
        return true;
    }
    if (starts_with(tokens[0], "SignalStrengthPercent"))
    {
        if (m_parent->m_ready)
            send_status(cmd, serial, "OK:100");
        else
            send_status(cmd, serial, "OK:20");
        return true;
    }
    if (starts_with(tokens[0], "LockTimeout"))
    {
        send_status(cmd, serial, "OK:12000");
        m_parent->OpenDev();
        return true;
    }
    if (starts_with(tokens[0], "HasTuner?"))
    {
        send_status(cmd, serial, "OK:No");
        return true;
    }
    if (starts_with(tokens[0], "HasPictureAttributes?"))
    {
        send_status(cmd, serial, "OK:No");
        return true;
    }
    if (starts_with(tokens[0], "SendBytes"))
    {
        // Used when FlowControl is Polling
        send_status(cmd, serial, "ERR:Not supported");
        return true;
    }
    if (starts_with(tokens[0], "XON"))
    {
        // Used when FlowControl is XON/XOFF
        send_status(cmd, serial, "OK");
        m_parent->m_xon = true;
        m_parent->m_flow_cond.notify_all();
        return true;
    }
    if (starts_with(tokens[0], "XOFF"))
    {
        send_status(cmd, serial, "OK");
        // Used when FlowControl is XON/XOFF
        m_parent->m_xon = false;
        m_parent->m_flow_cond.notify_all();
        return true;
    }
    if (starts_with(tokens[0], "IsOpen?"))
    {
        std::unique_lock<std::mutex> lk(m_parent->m_run_mutex);
        if (m_parent->m_fatal)
            send_status(cmd, serial, "ERR:" + m_parent->m_fatal_msg);
        else if (m_parent->m_ready)
            send_status(cmd, serial, "OK:Open");
        else
            send_status(cmd, serial, "WARN:Not Open yet");
        return true;
    }
    if (starts_with(tokens[0], "CloseRecorder"))
    {
        send_status(cmd, serial, "OK:Terminating");
        m_parent->Terminate();
        return true;
    }
    if (starts_with(tokens[0], "FlowControl?"))
    {
        send_status(cmd, serial, "OK:XON/XOFF");
        return true;
    }
    if (starts_with(tokens[0], "StartStreaming"))
    {
        string resultmsg;

        if (m_parent->StartEncoding(resultmsg))
            send_status(cmd, serial, "OK:Started");
        else
            send_status(cmd, serial, "ERR:" + resultmsg);
        return true;
    }
    if (starts_with(tokens[0], "StopStreaming"))
    {
        /* This does not close the stream!  When Myth is done with
         * this 'recording' ExternalChannel::EnterPowerSavingMode()
         * will be called, which invokes CloseRecorder() */
        string resultmsg;

        if (m_parent->StopEncoding(resultmsg))
            send_status(cmd, serial, "OK:Stopped");
        else
            send_status(cmd, serial, "ERR:" + resultmsg);
        return true;
    }

    // The following commands must have <data>
    if (tokens.size() < 2)
    {
        send_status(cmd, serial, "ERR:Malformed message. Expecting "
                    "<serial>:command:<data>, but received \"" +
                    cmd + "\"");
        return true;
    }

    if (starts_with(tokens[0], "APIVersion"))
    {
        std::unique_lock<std::mutex> lk(m_parent->m_run_mutex);
        m_api_version = stoul(tokens[1]);
        send_status(cmd, serial, "OK:API Version " +
                    std::to_string(m_api_version));
    }
    else if (starts_with(tokens[0], "TuneChannel"))
    {
        // Used if we announce that we have a 'tuner'
        send_status(cmd, serial, "ERR:Not supported");
    }
    else if (starts_with(tokens[0], "BlockSize"))
    {
        m_parent->BlockSize(stoul(tokens[1]));
        send_status(cmd, serial, "OK");
    }
    else
        send_status(cmd, serial, "ERR:Unrecognized command: '" +
                    tokens[0] + "'");

    return true;
}

void Commands::Run(void)
{
    string cmd;
    int    timeout = 250;

    int ret;
    int poll_cnt = 1;
    struct pollfd polls[2];
    memset(polls, 0, sizeof(polls));

    polls[0].fd      = 0;
    polls[0].events  = POLLIN | POLLPRI;
    polls[0].revents = 0;

    DEBUGLOG << "Command parser: ready.";

    while (m_parent->m_run)
    {
        ret = poll(polls, poll_cnt, timeout);

        if (polls[0].revents & POLLHUP)
        {
            ERRORLOG << "poll eof (POLLHUP)";
            break;
        }
        else if (polls[0].revents & POLLNVAL)
        {
            m_parent->Fatal("poll error");
            return;
        }

        if (polls[0].revents & POLLIN)
        {
            if (ret > 0)
            {
                std::getline(std::cin, cmd);
                if (!process_command(cmd))
                    m_parent->Fatal("Invalid command");
            }
            else if (ret < 0)
            {
                if ((EOVERFLOW == errno))
                {
                    ERRORLOG << "command overflow";
                    break; // we have an error to handle
                }

                if ((EAGAIN == errno) || (EINTR  == errno))
                {
                    ERRORLOG << "retry command read.";
                    continue; // errors that tell you to try again
                }

                ERRORLOG << "unknown error reading command.";
            }
        }
    }

    DEBUGLOG << "Command parser: shutting down";
}

Buffer::Buffer(MythTV * parent)
    : m_thread()
    , m_parent(parent)
    , m_run(true)
    , m_cb(std::bind(&Buffer::Fill, this, std::placeholders::_1,
                     std::placeholders::_2))
{
    m_heartbeat = std::chrono::system_clock::now();
}

void Buffer::Fill(void * data, size_t len)
{
    if (len < 1)
        return;

    static int dropped = 0;

    if (m_parent->m_flow_mutex.try_lock_for(std::chrono::seconds(2)))
    {
        if (m_data.size() < MAX_QUEUE)
        {
            block_t blk(reinterpret_cast<uint8_t *>(data),
                        reinterpret_cast<uint8_t *>(data) + len);

            m_data.push(blk);
            dropped = 0;
        }
        else if (++dropped % 25 == 0)
        {
            WARNLOG << "Packet queue overrun.  Dropped " << dropped
                    << "packets.";
        }

        m_parent->m_flow_mutex.unlock();
        m_parent->m_flow_cond.notify_all();
    }
    else
        WARNLOG << "Fill: Timed out on flow lock.";

    m_heartbeat = std::chrono::system_clock::now();
}

void Buffer::Run(void)
{
    bool       is_empty = false;
    bool       wait = false;
    time_t     send_time = time (NULL) + (60 * 5);
    uint64_t   write_total = 0;
    uint64_t   written = 0;
    uint64_t   write_cnt = 0;
    uint64_t   empty_cnt = 0;
    std::mutex tmp; // This is the only consumer, so can use local

    DEBUGLOG << "Buffer: Ready for data.";

    while (m_parent->m_run)
    {
        if (wait)
        {
            std::unique_lock<std::mutex> lk(tmp);
            m_parent->m_flow_cond.wait_for(lk, std::chrono::seconds(1));
            wait = false;
        }

        if (send_time < static_cast<double>(time (NULL)))
        {
            // Every 5 minutes, write out some statistics.
            send_time = time (NULL) + (60 * 5);
            write_total += written;
            if (m_parent->m_streaming)
                INFOLOG << "Count: " << write_cnt
                        << ", Empty cnt: " << empty_cnt
                        << ", Written: " << written
                        << ", Total: " << write_total;
            else
                INFOLOG << "Not streaming.";

            write_cnt = empty_cnt = written = 0;
        }

        if (m_parent->m_streaming)
        {
            if (m_parent->m_xon)
            {
                block_t pkt;

                if (m_parent->m_flow_mutex.try_lock_for(std::chrono::seconds(1)))
                {
                    if (!m_data.empty())
                    {
                        pkt = m_data.front();
                        m_data.pop();
                        is_empty = m_data.empty();
                    }
                    m_parent->m_flow_mutex.unlock();
                }

                if (!pkt.empty())
                {
                    write(1, pkt.data(), pkt.size());
                    written += pkt.size();
                    ++write_cnt;
                }

                if (is_empty)
                {
                    wait = true;
                    ++empty_cnt;
                }
            }
            else
                wait = true;
        }
        else
        {
            // Clear packet queue
            if (m_parent->m_flow_mutex.try_lock_for(std::chrono::seconds(1)))
            {
                if (!m_data.empty())
                {
                    stack_t dummy;
                    std::swap(m_data, dummy);
                }
                m_parent->m_flow_mutex.unlock();
            }

            wait = true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    DEBUGLOG << "Buffer: shutting down";
}
