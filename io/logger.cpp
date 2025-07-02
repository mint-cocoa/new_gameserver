#include "include/logger.h"
#include <filesystem>
#include <sstream>

namespace co_uring
{

    Logger &Logger::getInstance()
    {
        static Logger instance;
        return instance;
    }

    Logger::~Logger()
    {
        if (logFile_)
        {
            logFile_->close();
        }
    }

    void Logger::setLogFile(const std::string &filename)
    {
        std::lock_guard<std::mutex> lock(logMutex_);

        if (logFile_)
        {
            logFile_->close();
        }

        // 디렉토리 생성
        std::filesystem::path logPath(filename);
        if (logPath.has_parent_path())
        {
            std::filesystem::create_directories(logPath.parent_path());
        }

        logFile_ = std::make_unique<std::ofstream>(filename, std::ios::app);
        if (!logFile_->is_open())
        {
            std::cerr << "Failed to open log file: " << filename << std::endl;
            logFile_.reset();
        }
    }

    void Logger::logMessage(LogLevel level, const std::string &file, int line, const std::string &func, const std::string &message)
    {
        if (level < logLevel_)
        {
            return;
        }

        writeLog(level, file, line, func, message);
    }

    void Logger::writeLog(LogLevel level, const std::string &file, int line, const std::string &func, const std::string &message)
    {
        std::lock_guard<std::mutex> lock(logMutex_);

        std::string timestamp = getCurrentTime();
        std::string threadId = getThreadId();
        std::string levelStr = getLevelString(level);

        // 파일명에서 경로 제거
        std::string filename = file;
        if (!filename.empty())
        {
            size_t pos = filename.find_last_of("/\\");
            if (pos != std::string::npos)
            {
                filename = filename.substr(pos + 1);
            }
        }

        // 로그 메시지 구성
        std::ostringstream oss;
        oss << "[" << timestamp << "] "
            << "[" << levelStr << "] "
            << "[T:" << threadId << "] ";

        if (!filename.empty() && line > 0)
        {
            oss << "[" << filename << ":" << line;
            if (!func.empty())
            {
                oss << " " << func << "()";
            }
            oss << "] ";
        }

        oss << message;

        std::string logMessage = oss.str();

        // 콘솔 출력
        if (consoleOutput_)
        {
            if (colorOutput_)
            {
                std::cout << getColorCode(level) << logMessage << "\033[0m" << std::endl;
            }
            else
            {
                std::cout << logMessage << std::endl;
            }
        }

        // 파일 출력
        if (logFile_ && logFile_->is_open())
        {
            *logFile_ << logMessage << std::endl;
            logFile_->flush();
        }
    }

    std::string Logger::getCurrentTime()
    {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        oss << "." << std::setfill('0') << std::setw(3) << ms.count();

        return oss.str();
    }

    std::string Logger::getThreadId()
    {
        std::ostringstream oss;
        oss << std::this_thread::get_id();
        std::string id = oss.str();

        // Thread ID를 짧게 표시 (마지막 4자리만)
        if (id.length() > 4)
        {
            return id.substr(id.length() - 4);
        }
        return id;
    }

    std::string Logger::getLevelString(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::INFO:
            return "INFO ";
        case LogLevel::WARN:
            return "WARN ";
        case LogLevel::ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
        }
    }

    std::string Logger::getColorCode(LogLevel level)
    {
        if (!colorOutput_)
            return "";

        switch (level)
        {
        case LogLevel::DEBUG:
            return "\033[36m"; // Cyan
        case LogLevel::INFO:
            return "\033[32m"; // Green
        case LogLevel::WARN:
            return "\033[33m"; // Yellow
        case LogLevel::ERROR:
            return "\033[31m"; // Red
        default:
            return "\033[0m"; // Reset
        }
    }

} // namespace co_uring