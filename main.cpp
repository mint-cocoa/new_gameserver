#include "server/include/server.h"
#include "io/include/logger.h"
#include <iostream>
#include <signal.h>
#include <atomic>
#include <thread>
#include <chrono>

using namespace co_uring;

std::atomic<bool> shutdown_requested{false};

void signal_handler(int signal)
{
    std::cout << "\nShutdown signal received (" << signal << ")" << std::endl;
    shutdown_requested.store(true);
}

int main()
{
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    try
    {
        // Initialize logger
        auto &logger = co_uring::Logger::getInstance();
        logger.setLogLevel(co_uring::LogLevel::DEBUG);
        logger.setLogFile("logs/gameserver.log");
        logger.setColorOutput(true);

        LOG_INFO("=== Game Server Starting ===");

        // Create game server with hardware concurrency workers
        GameServer server(4); // Use 4 worker threads
        LOG_INFO("Created game server with {} worker threads", 4);

        // Start server on localhost:8080
        const char *host = "0.0.0.0";
        const std::uint16_t port = 8080;

        LOG_INFO("Starting server on {}:{}", host, port);

        if (!server.start(host, port))
        {
            LOG_ERROR("Failed to start game server");
            return 1;
        }

        LOG_INFO("Game server is running. Press Ctrl+C to stop.");

        // Wait for shutdown signal
        while (!shutdown_requested.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        LOG_INFO("Shutdown signal received, stopping server...");
        server.stop();

        LOG_INFO("=== Game Server Stopped ===");
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Server error: {}", e.what());
        return 1;
    }

    return 0;
}