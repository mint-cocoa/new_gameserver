#include "include/server.h"
#include "../io/include/logger.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <coroutine>
#include <thread>
#include "../coroutine/include/task.h"
#include "../session/include/service.h"

namespace co_uring
{

    void Worker::init(const char *host, std::uint16_t port)
    {
        LOG_DEBUG("Worker::init starting - host: {}, port: {}", host ? host : "null", port);

        // Initialize io_uring for this worker thread
        auto &io_uring = IoUring::getInstance();
        if (io_uring.queueInit() != 0)
        {
            LOG_ERROR("Failed to initialize io_uring queue");
            return;
        }
        LOG_DEBUG("io_uring queue initialized successfully");

        // Initialize buffer ring for this worker thread
        auto &buffer_ring = BufferRing::getInstance();
        if (buffer_ring.registerBufRing() != 0)
        {
            LOG_ERROR("Failed to register buffer ring");
            return;
        }
        LOG_DEBUG("Buffer ring registered successfully");

        // Create and setup server socket
        auto socket_server = bind(host, port);
        if (!socket_server)
        {
            LOG_ERROR("Failed to create server socket for {}:{}", host ? host : "null", port);
            return;
        }
        LOG_DEBUG("Server socket created successfully");

        if (socket_server->listen() != 0)
        {
            LOG_ERROR("Failed to listen on socket");
            return;
        }
        LOG_DEBUG("Socket listening successfully");

        LOG_INFO("Worker initialized on {}:{}", host ? host : "0.0.0.0", port);

        socket_server_ = std::make_unique<co_uring::socket_server>(std::move(*socket_server));

        // Start accepting clients (fire-and-forget)
        LOG_DEBUG("Starting accept_clients coroutine");
        spawn(accept_clients());
        LOG_DEBUG("Worker::init completed successfully");
    }

    void Worker::run()
    {
        LOG_DEBUG("Worker::run starting io_uring event loop");
        auto &io_uring = IoUring::getInstance();
        io_uring.eventLoop();
        LOG_DEBUG("Worker::run event loop ended");
    }

    auto Worker::accept_clients() -> task<void>
    {
        LOG_INFO("🔄 accept_clients coroutine started - managing client connections");

        if (!socket_server_)
        {
            LOG_ERROR("❌ socket_server_ is null in accept_clients");
            co_return;
        }

        LOG_INFO("🎯 Starting to accept client connections...");

        while (true)
        {
            LOG_DEBUG("⏳ Waiting for client connection...");
            auto client = co_await socket_server_->accept();
            if (client)
            {
                LOG_INFO("🔗 New client connected, creating session and spawning handler");
                auto session = std::make_shared<GameSession>(std::unique_ptr<socket_client>(client));
                session->OnConnected();
                spawn(GameSessionHandler::handle_client(session));
                LOG_DEBUG("🌱 Client handler coroutine spawned successfully");
            }
            else
            {
                LOG_WARN("⚠️ accept() returned null client");
            }
        }
    }

    auto Worker::handle_client(std::unique_ptr<socket_client> client) -> task<void>
    {
        // This function is now effectively replaced by the logic in accept_clients
        // It can be removed or left as a placeholder.
        co_return;
    }

    GameServer::GameServer(std::size_t worker_count) : worker_count_(worker_count)
    {
    }

    GameServer::~GameServer()
    {
        stop();
    }

    bool GameServer::start(const char *host, std::uint16_t port)
    {
        LOG_DEBUG("GameServer::start called with host: {}, port: {}", host ? host : "null", port);

        if (running_.load())
        {
            LOG_WARN("GameServer::start called but server is already running");
            return false;
        }

        running_.store(true);
        LOG_DEBUG("Set running flag to true");

        LOG_INFO("Starting game server on {}:{}", host ? host : "0.0.0.0", port);

        // Start worker threads
        for (std::size_t i = 0; i < worker_count_; ++i)
        {
            LOG_DEBUG("Starting worker thread {}/{}", i + 1, worker_count_);
            worker_threads_.emplace_back(&GameServer::worker_thread_func, this, host, port);
        }

        LOG_INFO("Game server started with {} workers", worker_count_);
        return true;
    }

    void GameServer::stop()
    {
        LOG_DEBUG("GameServer::stop called");

        if (!running_.load())
        {
            LOG_DEBUG("Server is not running, nothing to stop");
            return;
        }

        running_.store(false);
        LOG_DEBUG("Set running flag to false");

        LOG_INFO("Stopping worker threads...");
        for (auto &thread : worker_threads_)
        {
            if (thread.joinable())
            {
                LOG_DEBUG("Joining worker thread");
                thread.join();
            }
        }

        worker_threads_.clear();
        LOG_INFO("Game server stopped");
    }

    void GameServer::wait_for_shutdown()
    {
        LOG_DEBUG("GameServer::wait_for_shutdown called");

        for (auto &thread : worker_threads_)
        {
            if (thread.joinable())
            {
                LOG_DEBUG("Waiting for worker thread to finish");
                thread.join();
            }
        }

        LOG_DEBUG("All worker threads finished");
    }

    void GameServer::worker_thread_func(const char *host, std::uint16_t port)
    {
        LOG_DEBUG("Worker thread starting for {}:{}", host ? host : "null", port);

        Worker worker;
        worker.init(host, port);

        // Run the io_uring event loop
        LOG_DEBUG("Worker thread entering event loop");
        worker.run();

        LOG_DEBUG("Worker thread exiting");
    }
}