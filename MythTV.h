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

#ifndef _MythTV_H_
#define _MythTV_H_

#include "Common.h"
#include "HauppaugeDev.h"
#include "USBif.h"

#include <atomic>
#include <string>
#include <vector>
#include <queue>
#include <condition_variable>
#include <thread>
#include <mutex>

class MythTV;

class Buffer
{
  public:
    enum constants {MAX_QUEUE = 500};

    Buffer(MythTV * parent);
    ~Buffer(void) {
        m_run = false;
        if(m_thread.joinable()) m_thread.join();
    }
    void Start(void) {
        m_thread = std::thread(&Buffer::Run, this);
    }
    void Join(void) {
        if (m_thread.joinable())
            m_thread.join();
    }
    void SetBlockSize(uint32_t sz) { m_block_size = sz; }
    void Fill(void * data, size_t len);

    DataTransfer::callback_t & getWriteCallBack(void) { return m_cb; }
    std::chrono::time_point<std::chrono::system_clock> HeartBeat(void) const
    { return m_heartbeat; }

  protected:
    void Run(void);

  private:
    std::thread m_thread;
    MythTV     *m_parent;
    std::atomic_bool m_run;

    DataTransfer::callback_t m_cb;

    uint32_t m_block_size;

    using block_t = std::vector<uint8_t>;
    using stack_t = std::queue<block_t>;

    stack_t  m_data;

    std::chrono::time_point<std::chrono::system_clock> m_heartbeat;
};

class Commands
{
  public:
    enum { max_api_version = 2 };

    Commands(MythTV * parent);
    ~Commands(void) {
        m_run = false;
        if(m_thread.joinable()) m_thread.join();
    }
    void Start(void) {
        m_thread = std::thread(&Commands::Run, this);
    }
    void Join(void) {
        if (m_thread.joinable())
            m_thread.join();
    }

    bool send_status(const std::string & cmd, const std::string & serial,
                     const std::string & status);
    bool process_command(const std::string & cmd);

  protected:
    void Run(void);

  private:
    std::thread m_thread;
    std::atomic_bool m_run;

    MythTV* m_parent;
    int     m_api_version;
};

class MythTV
{
    friend class Buffer;
    friend class Commands;

  public:
    MythTV(const Parameters & params, const std::string & desc);
    ~MythTV(void);

    void Terminate(void);
    void Wait(void);
    void OpenDev(void);
    void Fatal(const std::string & msg);

    void BlockSize(uint32_t blocksz) { m_buffer.SetBlockSize(blocksz); }

    DataTransfer::callback_t & getWriteCallBack(void)
      { return m_buffer.getWriteCallBack(); }
    USBWrapper_t::callback_t & getErrorCallBack(void)
      { return m_error_cb; }

    bool StartEncoding(std::string & resultmsg);
    bool StopEncoding(std::string & resultmsg, bool soft = false);

    void USBError(void) { Fatal("Detected Error with USB."); }

  protected:
    std::string  m_desc;

    uint32_t     m_buffer_max;
    uint32_t     m_block_size;

    Buffer       m_buffer;
    Commands     m_commands;

    const Parameters   &m_params;
    HauppaugeDev       *m_dev;
    USBWrapper_t        m_usbio;

    std::atomic<bool> m_run;
    std::mutex   m_run_mutex;
    std::condition_variable m_run_cond;
    std::string  m_fatal_msg;
    bool         m_fatal;

    std::timed_mutex        m_flow_mutex;
    std::condition_variable m_flow_cond;
    std::atomic<bool> m_streaming;
    std::atomic<bool> m_xon;
    std::atomic<bool> m_ready;

    USBWrapper_t::callback_t m_error_cb;
};

#endif
