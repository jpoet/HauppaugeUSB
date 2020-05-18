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

#include "Logger.h"

#include <boost/log/core/core.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sinks/unlocked_frontend.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <fstream>
#include <ostream>

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace expr = boost::log::expressions;
namespace sinks = boost::log::sinks;
namespace attrs = boost::log::attributes;

static const char* SeverityStr[] =
{
    "DEBG",
    "INFO",
    "NOTC",
    "WARN",
    "ERRR",
    "CRIT"
};

bool DisableConsole = false;
void disableConsoleLog(void)
{
    DisableConsole = true;
}

std::string LogFilePath = "hauppauge2.log";
void setLogFilePath(const std::string & path)
{
    LogFilePath = path;
}

BOOST_LOG_ATTRIBUTE_KEYWORD(line_id, "LineID", unsigned int)
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(Severity, "Severity", SeverityLvl)
BOOST_LOG_ATTRIBUTE_KEYWORD(a_thread_name, "ThreadName", std::string)

// Allow a thread to declare its name
void setThreadName(const char *name)
{
    logging::core::get()->add_thread_attribute("ThreadName",
                               attrs::constant<std::string>(name));
}

void setLogLevelFilter(SeverityLvl level)
{
    CRITLOG << "Changing loglevel to " << SeverityStr[level];
    logging::core::get()->set_filter(Severity >= level);
}

void setLogLevelFilter(const std::string& lvl)
{
    std::string level(lvl);
    boost::to_upper(level);

    if (level == "CRITICAL")
        setLogLevelFilter(SeverityLvl::CRITICAL);
    else if (level == "ERROR")
        setLogLevelFilter(SeverityLvl::ERROR);
    else if (level == "WARNING")
        setLogLevelFilter(SeverityLvl::WARNING);
    else if (level == "NOTICE")
        setLogLevelFilter(SeverityLvl::NOTICE);
    else if (level == "INFO")
        setLogLevelFilter(SeverityLvl::INFO);
    else
        setLogLevelFilter(SeverityLvl::DEBUG);
}

template< typename CharT, typename TraitsT>
inline std::basic_istream< CharT, TraitsT >& operator>> (
    std::basic_istream< CharT, TraitsT >& strm, SeverityLvl &lvl)
{
    std::string tmp;
    strm >> tmp;

    // can make it case insensitive to allow 'warning' instead of only 'WARNING'
    // std::transform(tmp.begin(), tmp.end(), tmp.begin(), ::toupper);

    // if you have a lot of levels you may want use a map instead
    if (tmp.compare("INFO") == 0)
        lvl = SeverityLvl::INFO;
    else if (tmp.compare("NOTICE") == 0)
        lvl = SeverityLvl::NOTICE;
    else if (tmp.compare("WARNING") == 0)
        lvl = SeverityLvl::WARNING;
    else if (tmp.compare("ERROR") == 0)
        lvl = SeverityLvl::ERROR;
    else if (tmp.compare("CRITICAL") == 0)
        lvl = SeverityLvl::CRITICAL;
    // provide a default value for invalid strings
    else
        lvl = SeverityLvl::DEBUG;

    return strm;
}

// The operator is used for regular stream formatting
std::ostream& operator<< (std::ostream& strm, SeverityLvl level)
{
    if (static_cast< std::size_t >(level) <
        sizeof(SeverityStr) / sizeof(*SeverityStr))
        strm << SeverityStr[level];
    else
        strm << static_cast< int >(level);

    return strm;
}

// Attribute value tag type
struct severity_tag;

// The operator is used when putting the severity level to log
logging::formatting_ostream& operator<<
(
    logging::formatting_ostream& strm,
    logging::to_log_manip< SeverityLvl, severity_tag > const& manip
)
{
    SeverityLvl level = manip.get();
    if (static_cast< std::size_t >(level) <
        sizeof(SeverityStr) / sizeof(*SeverityStr))
        strm << SeverityStr[level];
    else
        strm << static_cast< int >(level);

    return strm;
}

BOOST_LOG_GLOBAL_LOGGER_INIT(logger, src::severity_logger_mt)
{
    src::severity_logger_mt<SeverityLvl> logger;

    logging::add_common_attributes();
    // add attributes

    // lines are sequentially numbered
    logger.add_attribute("LineID", attrs::counter<unsigned int>(1));
    // each log line gets a timestamp
    logger.add_attribute("TimeStamp", attrs::local_clock());

    // specify the format of the log message
    logging::formatter formatter =
        (expr::stream
         << expr::format_date_time<boost::posix_time::ptime>
         ("TimeStamp", "%Y-%m-%dT%H:%M:%S.%f") << " "
         << expr::attr< SeverityLvl, severity_tag >("Severity")
//         << boost::log::expressions::attr <boost::log::attributes::current_thread_id::value_type> ("ThreadID") << ", "
         << " <" << a_thread_name << "> "
         << expr::attr<std::string>("a_FileName")
         << ':' << expr::attr<int>("a_LineNum") << " ("
         << expr::attr<std::string>("a_Function")
         << ") "
         << expr::smessage);

    boost::shared_ptr< logging::core > core = logging::core::get();

    using backend_t = sinks::text_ostream_backend;
    using sink_t    = sinks::synchronous_sink< backend_t >;

    boost::shared_ptr< backend_t > backend(new backend_t());

    if (!DisableConsole)
    {
        backend->add_stream(boost::shared_ptr< std::ostream >
                            (&std::clog, boost::null_deleter()));
    }
    backend->add_stream(boost::shared_ptr< std::ostream >
                        (new std::ofstream(LogFilePath)));
    boost::shared_ptr< sink_t > file_sink(new sink_t(backend));
    file_sink->set_formatter(formatter);
    core->add_sink(file_sink);

    // Enable auto-flushing after each log record written
    backend->auto_flush(true);

    core->set_filter(Severity >= SeverityLvl::INFO);

    return logger;
}


/* For Hauppague src */

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

    // Remove trailing nl and/or cr
    while (!dst.empty() &&
           (dst[dst.length() - 1] == '\n' || dst[dst.length() - 1] == '\r'))
        dst.erase(dst.length() - 1);
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
