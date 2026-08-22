#pragma once

#include <iostream>
#include <fstream>
#include <string>

enum class LogLevel
{
    None,
    Info,
    Warn,
    Error
};

class Logger
{
public:
    static Logger &Instance();

    void Init(const std::string &filename = "agent.log");
    
    void Log(const std::string &msg, LogLevel level = LogLevel::Info);
    void LogInline(const std::string &msg, LogLevel level = LogLevel::Info);

private:
    Logger() = default;
    ~Logger();

    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    const char* GetLevelPrefix(LogLevel level) const;

    std::ofstream log_file_;
    bool is_start_of_line_{true};
};

#define NONE(msg)  Logger::Instance().Log(msg, LogLevel::None)
#define INFO(msg)  Logger::Instance().Log(msg, LogLevel::Info)
#define WARN(msg)  Logger::Instance().Log(msg, LogLevel::Warn)
#define ERROR(msg) Logger::Instance().Log(msg, LogLevel::Error)

#define NONE_INLINE(msg)  Logger::Instance().LogInline(msg, LogLevel::None)
#define INFO_INLINE(msg)  Logger::Instance().LogInline(msg, LogLevel::Info)
#define WARN_INLINE(msg)  Logger::Instance().LogInline(msg, LogLevel::Warn)
#define ERROR_INLINE(msg) Logger::Instance().LogInline(msg, LogLevel::Error)