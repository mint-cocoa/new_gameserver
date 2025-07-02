#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include <chrono>
#include <iomanip>

namespace co_uring
{

    enum class LogLevel
    {
        DEBUG = 0,
        INFO = 1,
        WARN = 2,
        ERROR = 3
    };

    class Logger
    {
    public:
        static Logger &getInstance();

        void setLogLevel(LogLevel level) { logLevel_ = level; }
        void setLogFile(const std::string &filename);
        void setConsoleOutput(bool enabled) { consoleOutput_ = enabled; }
        void setColorOutput(bool enabled) { colorOutput_ = enabled; }

        void logMessage(LogLevel level, const std::string &file, int line, const std::string &func, const std::string &message);

        template <typename... Args>
        void log(LogLevel level, const std::string &file, int line, const std::string &func, const std::string &format, Args &&...args)
        {
            if (level < logLevel_)
                return;

            std::ostringstream oss;
            formatHelper(oss, format, std::forward<Args>(args)...);
            logMessage(level, file, line, func, oss.str());
        }

    private:
        Logger() = default;
        ~Logger();

        void writeLog(LogLevel level, const std::string &file, int line, const std::string &func, const std::string &message);
        std::string getCurrentTime();
        std::string getThreadId();
        std::string getLevelString(LogLevel level);
        std::string getColorCode(LogLevel level);

        void formatHelper(std::ostringstream &oss, const std::string &format)
        {
            oss << format;
        }

        template <typename T, typename... Args>
        void formatHelper(std::ostringstream &oss, const std::string &format, T &&value, Args &&...args)
        {
            size_t pos = format.find("{}");
            if (pos != std::string::npos)
            {
                oss << format.substr(0, pos) << value;
                formatHelper(oss, format.substr(pos + 2), std::forward<Args>(args)...);
            }
            else
            {
                oss << format;
                if (sizeof...(args) > 0)
                {
                    oss << " ";
                    ((oss << args << " "), ...);
                }
            }
        }

        LogLevel logLevel_ = LogLevel::INFO;
        bool consoleOutput_ = true;
        bool colorOutput_ = true;
        std::unique_ptr<std::ofstream> logFile_;
        std::mutex logMutex_;
    };

// 매크로 정의
#define LOG_DEBUG(...) co_uring::Logger::getInstance().log(co_uring::LogLevel::DEBUG, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define LOG_INFO(...) co_uring::Logger::getInstance().log(co_uring::LogLevel::INFO, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define LOG_WARN(...) co_uring::Logger::getInstance().log(co_uring::LogLevel::WARN, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define LOG_ERROR(...) co_uring::Logger::getInstance().log(co_uring::LogLevel::ERROR, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

// 간단한 로깅 매크로
#define SIMPLE_LOG_DEBUG(...) co_uring::Logger::getInstance().logMessage(co_uring::LogLevel::DEBUG, "", 0, "", __VA_ARGS__)
#define SIMPLE_LOG_INFO(...) co_uring::Logger::getInstance().logMessage(co_uring::LogLevel::INFO, "", 0, "", __VA_ARGS__)
#define SIMPLE_LOG_WARN(...) co_uring::Logger::getInstance().logMessage(co_uring::LogLevel::WARN, "", 0, "", __VA_ARGS__)
#define SIMPLE_LOG_ERROR(...) co_uring::Logger::getInstance().logMessage(co_uring::LogLevel::ERROR, "", 0, "", __VA_ARGS__)

} // namespace co_uring