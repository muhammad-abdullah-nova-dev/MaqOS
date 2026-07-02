/*
 * ============================================================================
 *  MaqOS System Logger — Thread-Safe Singleton Logger
 * ============================================================================
 *  File:    logger.h
 *  Purpose: Provides a centralized, thread-safe logging system for MaqOS.
 *           Uses the Singleton pattern to ensure only one logger instance
 *           exists across the entire application.
 *
 *  Usage:
 *      LOG_INFO("System started");
 *      LOG_ERROR("Failed to allocate RAM");
 *      LOG_DEBUG("Process PID=1234 scheduled");
 *      LOG_WARN("RAM usage above 80%");
 *
 *  Output Format: [HH:MM:SS] [LEVEL] message
 *  Output Targets: Both "maqos_debug.log" file AND stdout (console)
 *
 *  Thread Safety: All log calls are protected by a std::mutex,
 *                 making it safe to call from any thread (scheduler,
 *                 deadlock monitor, main UI, etc.)
 * ============================================================================
 */
#pragma once
#include <iostream>
#include <fstream>    // ofstream for writing to the log file
#include <ctime>      // time(), localtime(), strftime() for timestamps
#include <string>
#include <mutex>      // std::mutex + std::lock_guard for thread safety

/*
 * Logger — Singleton class that writes timestamped log entries.
 *
 * Design Pattern: Singleton (Meyer's Singleton via static local variable)
 * - Private constructor ensures no external instantiation
 * - instance() returns a reference to the single static instance
 * - Log file is opened in append mode so logs persist across restarts
 */
class Logger {
    std::ofstream logFile;   // Output file stream for "maqos_debug.log"
    std::mutex mtx;          // Mutex to serialize concurrent log calls

    // Private constructor — opens the log file in append mode
    Logger() { logFile.open("maqos_debug.log", std::ios::app); }

public:
    // Returns the single Logger instance (created on first call, thread-safe in C++11+)
    static Logger& instance() { static Logger l; return l; }

    /*
     * log() — Core logging function.
     * Formats the message as "[HH:MM:SS] [LEVEL] message"
     * and writes it to both the log file and stdout.
     *
     * @param level  Log severity ("INFO", "ERROR", "DEBUG", "WARN")
     * @param msg    The message to log
     */
    void log(const std::string& level, const std::string& msg) {
        std::lock_guard<std::mutex> lock(mtx);  // Lock mutex for thread safety
        time_t now = time(nullptr);
        char buf[20];
        strftime(buf, sizeof(buf), "%H:%M:%S", localtime(&now));  // Format timestamp
        std::string line = "[" + std::string(buf) + "] [" + level + "] " + msg;
        logFile << line << "\n";
        logFile.flush();                 // Flush immediately so logs aren't lost on crash
        std::cout << line << std::endl;  // Also print to console for real-time monitoring
    }

    // Convenience methods for each log level
    void info(const std::string& m)  { log("INFO",  m); }
    void error(const std::string& m) { log("ERROR", m); }
    void debug(const std::string& m) { log("DEBUG", m); }
    void warn(const std::string& m)  { log("WARN",  m); }
};

// Macro shortcuts for cleaner logging syntax throughout the codebase
#define LOG_INFO(m)  Logger::instance().info(m)
#define LOG_ERROR(m) Logger::instance().error(m)
#define LOG_DEBUG(m) Logger::instance().debug(m)
#define LOG_WARN(m)  Logger::instance().warn(m)
