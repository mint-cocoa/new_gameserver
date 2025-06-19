#include "include/server.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <coroutine>
#include <thread>
#include "../coroutine/include/task.h"
#include "../session/include/service.h"

namespace co_uring {

void Worker::init(const char* host, std::uint16_t port) {
    // Initialize io_uring for this worker thread
    auto& io_uring = IoUring::getInstance();
    if (io_uring.queueInit() != 0) {
        std::cerr << "Failed to initialize io_uring queue" << std::endl;
        return;
    }
    
    // Initialize buffer ring for this worker thread
    auto& buffer_ring = BufferRing::getInstance();
    if (buffer_ring.registerBufRing() != 0) {
        std::cerr << "Failed to register buffer ring" << std::endl;
        return;
    }

    // Create and setup server socket
    auto socket_server = bind(host, port);
    if (!socket_server) {
        std::cerr << "Failed to create server socket" << std::endl;
        return;
    }
    
    if (socket_server->listen() != 0) {
        std::cerr << "Failed to bind and listen" << std::endl;
        return;
    }
    
    std::cout << "Worker initialized on " << host << ":" << port << std::endl;
    
    socket_server_ = std::make_unique<co_uring::socket_server>(std::move(*socket_server));
    
    // Start accepting clients
    spawn(accept_clients());
}

void Worker::run() {
    auto& io_uring = IoUring::getInstance();
    io_uring.eventLoop();
}

auto Worker::accept_clients() -> task<void> {
    if (!socket_server_) {
        co_return;
    }
    
    while (true) {
        auto client = co_await socket_server_->accept();
        if (client) {
            spawn(handle_client(std::unique_ptr<socket_client>(client)));
        }
    }
}

auto Worker::handle_client(std::unique_ptr<socket_client> client) -> task<void> {
    try {
        GameSessionHandler handler;
        co_await handler.handle_client(std::move(client));
    } catch (const std::exception& e) {
        std::cerr << "Error handling client: " << e.what() << std::endl;
    }
}

GameServer::GameServer(std::size_t worker_count) : worker_count_(worker_count) {
}

GameServer::~GameServer() {
    stop();
}

bool GameServer::start(const char* host, std::uint16_t port) {
    if (running_.load()) {
        return false;
    }
    
    running_.store(true);
    
    std::cout << "Starting game server on " << host << ":" << port << std::endl;
    
    // Start worker threads
    for (std::size_t i = 0; i < worker_count_; ++i) {
        worker_threads_.emplace_back(&GameServer::worker_thread_func, this, host, port);
    }
    
    std::cout << "Game server started with " << worker_count_ << " workers" << std::endl;
    return true;
}

void GameServer::stop() {
    running_.store(false);
    
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    worker_threads_.clear();
    std::cout << "Game server stopped" << std::endl;
}

void GameServer::wait_for_shutdown() {
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void GameServer::worker_thread_func(const char* host, std::uint16_t port) {
    Worker worker;
    worker.init(host, port);
    
    // Run the io_uring event loop
    worker.run();
} 
}