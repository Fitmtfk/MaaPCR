#include "logger.h"

Logger &Logger::Instance()
{
    static Logger instance;
    return instance;
}

Logger::~Logger()
{
    if (log_file_.is_open())
    {
        log_file_.close();
    }
}

const char *Logger::GetLevelPrefix(LogLevel level) const
{
    switch (level)
    {
    case LogLevel::None:
        return "";
    case LogLevel::Info:
        return "info:";
    case LogLevel::Warn:
        return "warn:";
    case LogLevel::Error:
        return "error:";
    default:
        return "info:";
    }
}

void Logger::Init(const std::string &filename)
{
    if (log_file_.is_open())
    {
        log_file_.close();
    }

    log_file_.open(filename, std::ios::out | std::ios::trunc);
    if (!log_file_.is_open())
    {
        std::cerr << "error:[Logger] 无法打开日志文件: " << filename << std::endl;
    }
}

void Logger::Log(const std::string &msg, LogLevel level)
{
    if (is_start_of_line_)
    {
        std::cout << GetLevelPrefix(level);
    }

    std::cout << msg << std::endl;
    std::cout.flush();
    is_start_of_line_ = true;

    if (log_file_.is_open())
    {
        log_file_ << msg << std::endl;
        log_file_.flush();
    }
}

void Logger::LogInline(const std::string &msg, LogLevel level)
{
    if (is_start_of_line_)
    {
        std::cout << GetLevelPrefix(level);
        is_start_of_line_ = false;
    }

    std::cout << msg;

    if (log_file_.is_open())
    {
        log_file_ << msg;
    }
}