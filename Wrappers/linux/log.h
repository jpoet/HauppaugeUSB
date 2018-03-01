#ifndef __LOG_H_
#define __LOG_H_

//#include <stdarg.h>
#include "../../Logger.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WRAPLOGL_ERROR = Logger::ERR,
    WRAPLOGL_WARN = Logger::WARNING,
    WRAPLOGL_NOTICE = Logger::NOTICE,
    WRAPLOGL_INFO = Logger::INFO,
    WRAPLOGL_DEBUG = Logger::DEBUG
} WRAPLOGL_t;

// wrapLog_vprintf(WRAPLOGL_t logLevel, const char *fmt, va_list args)
#define wrapLog_vprintf(LEVEL, FMT, ARGS) LOGFMT(LEVEL, FMT, ARGS);

//void wrapLog_printf(WRAPLOGL_t logLevel, const char *fmt, ...);
#define wrapLog_printf(LEVEL, FMT, ...) \
    LOGMSG(LEVEL, FmtString(FMT, ##__VA_ARGS__));

//void wrapLog_vprintf_ln(WRAPLOGL_t logLevel, const char *fmt, va_list args);
#define wrapLog_vprintf_ln(LEVEL, FMT, ARGS) LOGFMT(LEVEL, FMT, ARGS);

//void wrapLog_printf_ln(WRAPLOGL_t logLevel, const char *fmt, ...);
#define wrapLog_printf_ln(LEVEL, FMT, ...) \
    LOGMSG(LEVEL, FmtString(FMT, ##__VA_ARGS__));

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
