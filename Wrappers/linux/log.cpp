#include "log.h"

#include <boost/format.hpp>
#include <iostream>
#include <cstdarg>

#ifdef USEBOOSTFORMAT

template<typename... Arguments>
  std::string FormatArgs(const std::string & fmt, Arguments&&... args)
{
#ifdef __cpp_fold_expressions
    return boost::str((boost::format(fmt) % ... % args));
#else
    boost::format f(fmt);
    std::initializer_list<char> {(static_cast<void>
                                  ( f % args ), char{}) ...};
    std::cerr << boost::str(f);
    return boost::str(f);
#endif
}

#else

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

#endif
