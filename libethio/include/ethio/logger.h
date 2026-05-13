// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Moab

#pragma once

#include <cstdarg>
#include <cstdio>
#include <string>

namespace ethio {

class Logger {
public:
    Logger() = default;
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void enable(const std::string& filepath);
    void disable();

    bool enabled() const { return file_ != nullptr; }

    void debug(const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
        __attribute__((format(printf, 2, 3)))
#endif
        ;

    void info(const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
        __attribute__((format(printf, 2, 3)))
#endif
        ;

    void warning(const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
        __attribute__((format(printf, 2, 3)))
#endif
        ;

private:
    void vlog(const char* level, const char* fmt, va_list ap);

    FILE* file_ = nullptr;
    bool owned_ = false;
};

extern Logger logger;

} // namespace ethio
