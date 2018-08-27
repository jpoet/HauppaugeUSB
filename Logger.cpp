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

//#define CONFIG_SYSTEMD_JOURNAL 0
#if CONFIG_SYSTEMD_JOURNAL
#define SD_JOURNAL_SUPPRESS_LOCATION 1 // Manage location ourselves.
#include <systemd/sd-journal.h>
#endif

#define BOOST_FILESYSTEM_NO_DEPRECATED 1
#include <boost/filesystem.hpp>

#include <iostream>
#include <sstream>

#include "Logger.h"

#include <ctime>

#include <libgen.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <iomanip>
#include <cassert>

using namespace std;
using namespace std::chrono;

// Global static pointer used to ensure a single instance of the class.
Logger* Logger::m_instance = NULL;

Logger::Logger(void)
    : std::ostream(&m_stream_buf)
    , m_pid(-1)
    , m_log_level_filt(Logger::INFO)
    , m_suppressLoc(false)
    , m_use_journal(false)
    , m_quiet(false)
    , m_stream_buf(this)
{
    m_log_start = steady_clock::now();
    m_pid = getpid();
}

Logger* Logger::Get(void)
{
    if (!m_instance)   // Only allow one instance of class to be generated.
        m_instance = new Logger;
    return m_instance;
}

void Logger::setThreadName(const std::string& name)
{
    pthread_setname_np(pthread_self(), name.c_str());
}

void Logger::setAppName(const string& aname)
{
    boost::lock_guard<boost::mutex> lock(m_mutex);

    m_app_name = aname;
}

void Logger::suppressLoc(bool suppress)
{
    boost::lock_guard<boost::mutex> lock(m_mutex);

    m_suppressLoc = suppress;
}

void Logger::setFilter(const string& lvl)
{
    boost::lock_guard<boost::mutex> lock(m_mutex);

    if (lvl == "emerg")
        m_log_level_filt = Logger::EMERG;
    else if (lvl == "alert")
        m_log_level_filt = Logger::ALERT;
    else if (lvl == "crit")
        m_log_level_filt = Logger::CRIT;
    else if (lvl == "err")
        m_log_level_filt = Logger::ERR;
    else if (lvl == "warning")
        m_log_level_filt = Logger::WARNING;
    else if (lvl == "notice")
        m_log_level_filt = Logger::NOTICE;
    else if (lvl == "info")
        m_log_level_filt = Logger::INFO;
    else
        m_log_level_filt = Logger::DEBUG;
}

void Logger::setFilter(int lvl)
{
    m_log_level_filt = static_cast<Levels>(std::min(lvl,
                                           static_cast<int>(Logger::DEBUG)));
}

bool Logger::setFile(const string& fname)
{
    boost::lock_guard<boost::mutex> lock(m_mutex);

    boost::filesystem::path path(fname);

    m_file_name = path.string();
    m_file.close();

    if (!boost::filesystem::is_directory(path.parent_path()))
    {
        boost::system::error_code ec;
        if (!boost::filesystem::create_directories(path.parent_path(), ec) &&
            ec.value() != boost::system::errc::success)
        {
            cerr << "Unable to create path " << path.parent_path() << ": "
                 << ec.message() << endl;
            m_file_name.clear();
            return false;
        }
    }

    m_file.open(m_file_name, ios::app);
    if (!m_file.is_open())
    {
        cerr << "Unable to open log file '" << m_file_name << "': "
             << strerror(errno) << endl;
        m_file_name.clear();
        return false;
    }

    return true;
}

void Logger::setJournal(bool enable)
{
    boost::lock_guard<boost::mutex> lock(m_mutex);

    m_use_journal = enable;
}

void Logger::setQuiet(bool enable)
{
    boost::lock_guard<boost::mutex> lock(m_mutex);

    m_quiet = enable;
}

void Logger::Output(std::chrono::system_clock::time_point& now,
                    std::chrono::duration<int64_t, std::nano>& offset,
                    const string& file, const string& function,
                    int line, int level, const string& msg, bool sync)
{
    if (sync && m_stream_buf.in_avail())
        m_stream_buf.sync();

    char thread_name[16];
    bool have_thread = pthread_getname_np(pthread_self(), thread_name, 16) == 0;

    char level_code;
    switch (level)
    {
        case Logger:: DEBUG:
          level_code = 'D';
          break;
        case Logger:: INFO:
          level_code = 'I';
          break;
        case Logger:: NOTICE:
          level_code = 'N';
          break;
        case Logger:: WARNING:
          level_code = 'W';
          break;
        case Logger:: ERR:
          level_code = 'E';
          break;
        case Logger:: CRIT:
          level_code = 'C';
          break;
        default:
          level_code = 'U';
          break;
    }

    ostringstream os;

    if (!m_suppressLoc)
    {
        if (have_thread)
            os << '[' << thread_name << "] ";
        os << basename(const_cast<char*>(file.c_str())) << ':' << line
           << " (" << function << ") - ";
    }
    os << msg;


#if CONFIG_SYSTEMD_JOURNAL
    if (m_use_journal)
    {
        sd_journal_send("MESSAGE=%s", os.str().c_str(),
                        "PRIORITY=%d", level,
                        "CODE_FILE=%s", file.c_str(),
                        "CODE_LINE=%d", line,
                        "CODE_FUNC=%s", function.c_str(),
                        "SYSLOG_IDENTIFIER=%s", m_app_name.c_str(),
                        "SYSLOG_PID=%d", m_pid,
                        "THREAD=%s", thread_name,
                        NULL);
    }
#endif

    if (level > m_log_level_filt)
        return;

    if (m_file.is_open())
    {
        ostringstream flos;

        time_t tt = system_clock::to_time_t(now);
//    tm utc_tm = *gmtime(&tt);
        tm local_tm = *localtime(&tt);
        flos.fill('0');
        flos << setw(4) << local_tm.tm_year + 1900 << '-'
             << setw(2) << local_tm.tm_mon + 1 << '-'
             << setw(2) << local_tm.tm_mday << ' '
             << setw(2) << local_tm.tm_hour << ':'
             << setw(2) << local_tm.tm_min << ':'
             << setw(2) << local_tm.tm_sec;

        // Get microseconds
        system_clock::duration tp = now.time_since_epoch();
        tp -= duration_cast<seconds>(tp);
        flos << '.' << setw(6) << duration_cast<microseconds>(tp).count();
        m_file << flos.str() << ' ' << level_code << ' ' << os.str() << endl;
    }

    if (!m_quiet)
    {
        ostringstream console;

        auto hh = duration_cast<hours>(offset);
        offset -= hh;
        auto mm = duration_cast<minutes>(offset);
        offset -= mm;
        auto ss = duration_cast<seconds>(offset);
        offset -= ss;
        auto ff = duration_cast<microseconds>(offset);

        console.fill('0');
        console << setw(3) << hh.count() << ':' << setw(2) << mm.count()
                << ':' << setw(2) << ss.count() << '.' << setw(6) << ff.count();

        cerr << console.str() << ' ' << level_code << setw(6) << m_pid << ' ' << os.str() << endl;
    }
}

///
/// \breif Format message
/// \param dst String to store formatted message
/// \param format Format of message
/// \param ap Variable argument list
///
void FmtString(std::string &dst, const char *format, va_list ap) throw()
{
    int length;
    va_list apStrLen;
    va_copy(apStrLen, ap);
    length = vsnprintf(NULL, 0, format, apStrLen);
    va_end(apStrLen);
    if (length > 0) {
        dst.resize(length);
        vsnprintf((char *)dst.data(), dst.size() + 1, format, ap);
    } else {
        dst = "Format error! format: ";
        dst.append(format);
    }
}

///
/// \breif Format message
/// \param dst String to store formatted message
/// \param format Format
/// of message \param ... Variable argument list
///
void FmtString(std::string &dst, const char *format, ...) throw()
{
    va_list ap;
    va_start(ap, format);
    FmtString(dst, format, ap);
    va_end(ap);
}

///
/// \breif Format message
/// \param format Format of message
/// \param ... Variable argument list
///
std::string FmtString(const char *format, ...) throw()
{
    std::string dst;
    va_list ap;
    va_start(ap, format);
    FmtString(dst, format, ap);
    va_end(ap);
    return dst;
}

///
/// \breif Format message
/// \param format Format of message
/// \param ap Variable argument list
///
std::string FmtString(const char *format, va_list ap) throw()
{
    std::string dst;
    FmtString(dst, format, ap);
    return dst;
}
