#include "ethio/logger.h"

#include <ctime>

namespace ethio {

Logger::~Logger()
{
    if (file_ && owned_) {
        fclose(file_);
    }
    file_ = nullptr;
    owned_ = false;
}

void Logger::enable(const std::string& filepath)
{
    if (file_ && owned_) {
        fclose(file_);
    }
    file_ = fopen(filepath.c_str(), "w");
    owned_ = (file_ != nullptr);
    if (file_) {
        setbuf(file_, nullptr);
    }
}

void Logger::disable()
{
    if (file_ && owned_) {
        fclose(file_);
    }
    file_ = nullptr;
    owned_ = false;
}

void Logger::vlog(const char* level, const char* fmt, va_list ap)
{
    if (!file_) return;

    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);

    fprintf(file_, "%04d-%02d-%02d %02d:%02d:%02d [%-7s] ",
            tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
            tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
            level);

    va_list ap_copy;
    va_copy(ap_copy, ap);
    vfprintf(file_, fmt, ap_copy);
    va_end(ap_copy);

    fprintf(file_, "\n");
    fflush(file_);
}

void Logger::debug(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlog("DEBUG", fmt, ap);
    va_end(ap);
}

void Logger::info(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlog("INFO", fmt, ap);
    va_end(ap);
}

void Logger::warning(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlog("WARNING", fmt, ap);
    va_end(ap);
}

Logger logger;

} // namespace ethio
