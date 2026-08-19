#include "logger.h"

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void Logger::Init(const std::string& filename) {
    if (log_file_.is_open()) {
        log_file_.close();
    }

    log_file_.open(filename, std::ios::out | std::ios::trunc);
    if (!log_file_.is_open()) {
        std::cerr << "[Logger] 无法打开日志文件: " << filename << std::endl;
    }
}

void Logger::Log(const std::string& msg) {
    std::cout << msg << std::endl;

    if (log_file_.is_open()) {
        log_file_ << msg << std::endl;
        log_file_.flush();
    }
}

void Logger::LogInline(const std::string& msg) {
    std::cout << msg;
    if (log_file_.is_open()) {
        log_file_ << msg;
    }
}