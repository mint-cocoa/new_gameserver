#include "server/include/server.h"
#include <iostream>
#include <signal.h>
#include <atomic>
#include <thread>
#include <chrono>

using namespace co_uring;

std::atomic<bool> shutdown_requested{false};

void signal_handler(int signal) {
    std::cout << "\nShutdown signal received (" << signal << ")" << std::endl;
    shutdown_requested.store(true);
}

int main() {
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    try {
        std::cout << "=== Game Server Starting ===" << std::endl;
        
        // Create game server with hardware concurrency workers
        GameServer server(4); // Use 4 worker threads
        
        // Start server on localhost:8080
        const char* host = "0.0.0.0";
        const std::uint16_t port = 8080;
        
        if (!server.start(host, port)) {
            std::cerr << "Failed to start game server" << std::endl;
            return 1;
        }
        
        std::cout << "Game server is running. Press Ctrl+C to stop." << std::endl;
        
        // Wait for shutdown signal
        while (!shutdown_requested.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        std::cout << "Shutting down server..." << std::endl;
        server.stop();
        
        std::cout << "=== Game Server Stopped ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}