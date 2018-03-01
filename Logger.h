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

#ifndef _Logger_h_
#define _Logger_h_

#include <chrono>

#include <boost/thread/thread.hpp>

#include <iostream>
#include <fstream>
#include <sstream>

#include <stdarg.h>
#include <string>

class Logger : public std::ostream
{
    class LogStreamBuf: public std::stringbuf
    {
      public:
        LogStreamBuf(Logger* parent) { m_parent = parent; }

        // On sync, output to log
        virtual int sync(void)
        {
            m_parent->Log(str());
            str("");
            m_parent->Unlock();
            return 0;
        }

      private:
        LogStreamBuf(void);

        Logger* m_parent;
    };

    friend class LogStreamBuf;

  public:
    enum Levels {EMERG, ALERT, CRIT, ERR, WARNING, NOTICE, INFO, DEBUG};

    static Logger* Get(void);
    static void setThreadName(const std::string& name);

    void setAppName(const std::string& app_name);
    void suppressLoc(bool suppress);
    void setFilter(const std::string& lvl);
    void setFilter(int lvl);
    bool setFile(const std::string& fname);
    void setJournal(bool enable);
    void setQuiet(bool enable);

    void LogMsg(const std::string& file, const std::string& function,
                int line, int level, const std::string& msg)
    {
        std::chrono::system_clock::time_point now
            = std::chrono::system_clock::now();
        std::chrono::duration<int64_t, std::nano> offset
            = std::chrono::steady_clock::now() - m_log_start;

        boost::lock_guard<boost::mutex> lock(m_mutex);
        Output(now, offset, file, function, line, level, msg, true);
    }
    std::ostream& Location(const std::string& file, const std::string& function,
                  int line, int level)
    {
        m_loc_file = file;
        m_loc_function = function;
        m_loc_line = line;
        m_level = level;
        return *this;
    }

  protected:
    void Log(const std::string& msg)
    {
        std::chrono::system_clock::time_point now
            = std::chrono::system_clock::now();
        std::chrono::duration<int64_t, std::nano> offset
            = std::chrono::steady_clock::now() - m_log_start;

        m_mutex.lock();
        Output(now, offset, m_loc_file, m_loc_function,
               m_loc_line, m_level, msg, false);
    }

    void Unlock(void)
    {
        m_mutex.unlock(); // Lock held in Log(const std::string& msg)
    }

  private:
    Logger(void);

    void Output(std::chrono::system_clock::time_point& now,
                std::chrono::duration<int64_t, std::nano>& offset,
                const std::string& file, const std::string& function,
                int line, int level, const std::string& msg, bool sync);

    pid_t        m_pid;
    std::string  m_app_name;
    std::string  m_file_name;
    std::ofstream m_file;
    int          m_log_level_filt;
    bool         m_suppressLoc;
    bool         m_use_journal;
    bool         m_quiet;
    LogStreamBuf m_stream_buf;
    std::chrono::steady_clock::time_point m_log_start;

    std::string  m_loc_file;
    std::string  m_loc_function;
    int          m_loc_line;
    int          m_level;

    boost::mutex   m_mutex;
    static Logger* m_instance;
};

///
/// \breif Format message
/// \param dst String to store formatted message
/// \param format Format of message
/// \param ap Variable argument list
///
void FmtString(std::string &dst, const char *format, va_list ap) throw();

///
/// \breif Format message
/// \param dst String to store formatted message
/// \param format Format
/// of message \param ... Variable argument list
///
void FmtString(std::string &dst, const char *format, ...) throw();

///
/// \breif Format message
/// \param format Format of message
/// \param ... Variable argument list
///
std::string FmtString(const char *format, ...) throw();

///
/// \breif Format message
/// \param format Format of message
/// \param ap Variable argument list
///
std::string FmtString(const char *format, va_list ap) throw();

#define LOG(LEVEL) \
    Logger::Get()->Location(__FILE__, __FUNCTION__, __LINE__, LEVEL)

#define LOGMSG(LEVEL, ARG) Logger::Get()->LogMsg(__FILE__, __FUNCTION__, __LINE__, LEVEL, ARG)

#define LOGFMT(LEVEL, FMT, ARGS) LOGMSG(LEVEL, FmtString(FMT, ARGS))

#define LOGGER_DEBUG(ARG) LOGMSG(Logger::DEBUG, ARG);
#define LOGGER_INFO(ARG)  LOGMSG(Logger::INFO, ARG);
#define LOGGER_WARN(ARG)  LOGMSG(Logger::WARNING, ARG);
#define LOGGER_ERROR(ARG) LOGMSG(Logger::ERR, ARG);
#define LOGGER_FATAL(ARG) LOGMSG(Logger::CRIT, ARG);

#endif
