#ifndef __LOG_H_
#define __LOG_H_

//#include <stdarg.h>
#include "../../Logger.h"

typedef enum {
    WRAPLOGL_ERROR  = SeverityLvl::ERROR,
    WRAPLOGL_WARN   = SeverityLvl::WARNING,
    WRAPLOGL_NOTICE = SeverityLvl::NOTICE,
    WRAPLOGL_INFO   = SeverityLvl::INFO,
    WRAPLOGL_DEBUG  = SeverityLvl::DEBUG
} WRAPLOGL_t;


// This needs revisited in the future...
// #define USEBOOSTFORMAT

#ifdef USEBOOSTFORMAT

template<typename... Arguments>
  std::string FormatArgs(const std::string & fmt, Arguments&&... args);

#define LOGFMT(LEVEL, FMT, ARGS) LOGMSG(LEVEL, FormatArgs(FMT, ARGS))

#else
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

#if 0
inline char* chomp(char *str)
{
    int l;

    for(;;)
    {
        l = strlen(str);
        if (str[l-1] != '\n')
            return str;
        str[l-1] = 0;
    }
    return str;
}
#endif

#define LOGFMT(LEVEL, FMT, ARGS) LOGMSG(LEVEL, FmtString(FMT, ARGS))

#endif // USEBOOSTFORMAT

#define LOGMSG(LEVEL, ARG) LOG(SeverityLvl(LEVEL)) << ARG

#define LOGGER_DEBUG(ARG)  LOGMSG(DEBUG, ARG);
#define LOGGER_INFO(ARG)   LOGMSG(INFO, ARG);
#define LOGGER_NOTICE(ARG) LOGMSG(NOTICE, ARG);
#define LOGGER_WARN(ARG)   LOGMSG(WARNING, ARG);
#define LOGGER_ERROR(ARG)  LOGMSG(ERROR, ARG);
#define LOGGER_FATAL(ARG)  LOGMSG(CRITICAL, ARG);

#ifdef __cplusplus
extern "C" {
#endif




// wrapLog_vprintf(WRAPLOGL_t logLevel, const char *fmt, va_list args)
#define wrapLog_vprintf(LEVEL, FMT, ARGS) LOGFMT(LEVEL, FMT, ARGS);

//void wrapLog_printf(WRAPLOGL_t logLevel, const char *fmt, ...);
#ifdef USEBOOSTFORMAT

#define wrapLog_printf(LEVEL, FMT, ...) \
    LOGMSG(LEVEL, FormatArgs(FMT, ##__VA_ARGS__));
#define wrapLog_printf_ln(LEVEL, FMT, ...) \
    LOGMSG(LEVEL, FormatArgs(FMT, ##__VA_ARGS__));

#else

#define wrapLog_printf(LEVEL, FMT, ...) \
    LOGMSG(LEVEL, FmtString(FMT, ##__VA_ARGS__));
#define wrapLog_printf_ln(LEVEL, FMT, ...) \
    LOGMSG(LEVEL, FmtString(FMT, ##__VA_ARGS__));

#endif

#define wrapLog_vprintf_ln(LEVEL, FMT, ARGS) LOGFMT(LEVEL, FMT, ARGS);

#define WRAPLOGV wrapLog_vprintf_ln
#define WRAPLOG wrapLog_printf_ln

#define wrapLogError(M, ...) WRAPLOG(WRAPLOGL_ERROR, M, ##__VA_ARGS__)
#define wrapLogWarn(M, ...) WRAPLOG(WRAPLOGL_WARN, M, ##__VA_ARGS__)
#define wrapLogNotice(M, ...) WRAPLOG(WRAPLOGL_NOTICE, M, ##__VA_ARGS__)
#define wrapLogInfo(M, ...) WRAPLOG(WRAPLOGL_INFO, M, ##__VA_ARGS__)
#define wrapLogDebug(M, ...) WRAPLOG(WRAPLOGL_DEBUG, M, ##__VA_ARGS__)

#define _DbgLogError(M, ...) wrapLog_printf(WRAPLOGL_ERROR, M, ##__VA_ARGS__)
#define _DbgLogWarn(M, ...) wrapLog_printf(WRAPLOGL_WARN, M, ##__VA_ARGS__)
#define _DbgLogNotice(M, ...) wrapLog_printf(WRAPLOGL_NOTICE, M, ##__VA_ARGS__)
#define _DbgLogInfo(M, ...) wrapLog_printf(WRAPLOGL_INFO, M, ##__VA_ARGS__)
#define _DbgLogRelease(M, ...) wrapLog_printf(WRAPLOGL_INFO, M, ##__VA_ARGS__)
#define _DbgLogTrace(M, ...) wrapLog_printf(WRAPLOGL_DEBUG, M, ##__VA_ARGS__)
#define _DbgLogMicroTrace(M, ...) wrapLog_printf(WRAPLOGL_DEBUG, M, ##__VA_ARGS__)

#define DbgLogError(V) _DbgLogError V
#define DbgLogWarn(V) _DbgLogWarn V
#define DbgLogNotice(V) _DbgLogNotice V
#define DbgLogInfo(V) _DbgLogInfo V
#define DbgLogRelease(V) _DbgLogRelease V
#define DbgLogTrace(V) _DbgLogTrace V
#define DbgLogMicroTrace(V) _DbgLogMicroTrace V

#define DbgLog DbgLogInfo

#ifdef __cplusplus
}
#endif

#endif
