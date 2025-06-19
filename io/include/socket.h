#pragma once

#include "file.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <coroutine>
#include <memory>
#include <optional>
#include <tuple>
#include <cstdint>
#include <cstddef>
#include <liburing.h>
#include "../../coroutine/include/task.h"
#include "io_uring.h"

namespace co_uring {

class sqe_data;

class socket_client : public file {
public:
    using file::file;

    class recv_awaiter {
    public:
        recv_awaiter(std::uint32_t raw_fd) noexcept;
        
        [[nodiscard]] bool await_ready() const noexcept;
        void await_suspend(std::coroutine_handle<> coroutine) noexcept;
        [[nodiscard]] int await_resume() noexcept;
        [[nodiscard]] std::uint32_t get_buffer_id() const noexcept { return buffer_id_; }
        [[nodiscard]] std::uint32_t get_buffer_size() const noexcept { return buffer_size_; }

    private:
        mutable sqe_data sqe_data_;
        const std::uint32_t raw_fd_;
        mutable std::uint32_t buffer_id_{0};
        mutable std::uint32_t buffer_size_{0};
    };

    [[nodiscard]] recv_awaiter recv() const noexcept;

    class send_awaiter {
    public:
        send_awaiter(std::uint32_t raw_fd, const std::uint8_t* buf, std::size_t size) noexcept;
        
        [[nodiscard]] bool await_ready() const noexcept;
        void await_suspend(std::coroutine_handle<> coroutine) noexcept;
        [[nodiscard]] int await_resume() const noexcept;

    private:
        mutable sqe_data sqe_data_;
        const std::uint32_t raw_fd_;
        const std::uint8_t* buffer_;
        const std::size_t buffer_size_;
    };

    [[nodiscard]] auto send(const std::uint8_t* buf, std::size_t size) const noexcept -> task<int>;
};

class socket_server : public file {
public:
    using file::file;

    [[nodiscard]] int listen(int backlog = 128) const noexcept;

    class multishot_accept_guard {
    public:
        multishot_accept_guard(std::uint32_t raw_fd, 
                               sockaddr_storage* client_address,
                               socklen_t* client_address_size) noexcept;
        ~multishot_accept_guard() noexcept;

        multishot_accept_guard(const multishot_accept_guard&) = delete;
        multishot_accept_guard& operator=(const multishot_accept_guard&) = delete;
        multishot_accept_guard(multishot_accept_guard&&) = default;
        multishot_accept_guard& operator=(multishot_accept_guard&&) = default;

        [[nodiscard]] bool await_ready() const noexcept;
        void await_suspend(std::coroutine_handle<> coroutine) noexcept;
        [[nodiscard]] socket_client* await_resume() const noexcept;

    private:
        mutable sqe_data sqe_data_;
        mutable bool initial_await_;
        const std::uint32_t raw_fd_;
        sockaddr_storage* const client_address_;
        socklen_t* const client_address_size_;
    };

    [[nodiscard]] multishot_accept_guard& accept(sockaddr_storage* client_address = nullptr,
                                                 socklen_t* client_address_size = nullptr) noexcept;

private:
    std::optional<multishot_accept_guard> multishot_accept_guard_;
};

// Free function to create and bind a socket server
[[nodiscard]] auto bind(const char* host = nullptr, std::uint16_t port = 8080) noexcept -> std::optional<socket_server>;

} // namespace co_uring