#pragma once

#include <iostream>
#include <fstream>
#include <string>

class Logger {
public:
    static Logger& Instance();

    void Init(const std::string& filename = "agent.log");
    void Log(const std::string& msg);
    void LogInline(const std::string& msg);

private:
    Logger() = default;
    ~Logger();

    // 禁用拷贝与赋值操作
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::ofstream log_file_;
};

// 快捷调用宏
#define LOG(msg) Logger::Instance().Log(msg)
#define LOG_INLINE(msg) Logger::Instance().LogInline(msg)