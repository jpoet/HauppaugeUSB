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

#ifndef _logger_h_
#define _logger_h_

// necessary when linking the boost_log library dynamically
#define BOOST_LOG_DYN_LINK 1

#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>

namespace logging = boost::log;
namespace attrs = boost::log::attributes;
namespace keywords = boost::log::keywords;

// We define our own severity levels
enum SeverityLvl
{
    DEBUG,
    INFO,
    NOTICE,
    WARNING,
    ERROR,
    CRITICAL
};

// Initial logging severity threshold
#define SEVERITY_THRESHOLD CRITICAL

// Allow a thread to declare its name
void setThreadName(const char *name);
// Change log level
void setLogLevelFilter(SeverityLvl level);
void setLogLevelFilter(const std::string& lvl);
void disableConsoleLog(void);
void setLogFilePath(const std::string & path);

// register a global logger
BOOST_LOG_GLOBAL_LOGGER(logger,
                        logging::sources::severity_logger_mt<SeverityLvl>)


// Convert file path to only the filename
inline std::string strippath(std::string path) {
   return path.substr(path.find_last_of("/")+1);
}

#define LOG(sev)         \
    BOOST_LOG_SEV(logger::get(), sev)           \
    << logging::add_value("a_FileName", strippath(__FILE__))    \
    << logging::add_value("a_LineNum", __LINE__) \
    << logging::add_value("a_Function", __FUNCTION__)


// ===== log macros =====
#define DEBUGLOG  LOG(SeverityLvl::DEBUG)
#define INFOLOG   LOG(SeverityLvl::INFO)
#define NOTICELOG LOG(SeverityLvl::NOTICE)
#define WARNLOG   LOG(SeverityLvl::WARNING)
#define ERRORLOG  LOG(SeverityLvl::ERROR)
#define CRITLOG   LOG(SeverityLvl::CRITICAL)

#endif // _logger_h_
