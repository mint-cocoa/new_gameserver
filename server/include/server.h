#pragma once

#include "../../io/include/socket.h"
#include "../../io/include/buffer_ring.h"
#include "../../io/include/io_uring.h"
#include "../../coroutine/include/task.h"
#include "../../session/include/session.h"
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <ranges>

namespace co_uring {

class Worker {
public:
    void init(const char* host, std::uint16_t port);
    void run();
    auto accept_clients() -> task<void>;
    auto handle_client(std::unique_ptr<socket_client> client) -> task<void>;

private:
    std::unique_ptr<socket_server> socket_server_;
};

class GameServer {
public:
    explicit GameServer(std::size_t worker_count);
    ~GameServer();
    
    bool start(const char* host, std::uint16_t port);
    void stop();
    void wait_for_shutdown();
    
private:
    void worker_thread_func(const char* host, std::uint16_t port);
    
    std::size_t worker_count_;
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> running_{false};
};

// Helper template functions

} // namespace co_uring