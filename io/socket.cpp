#include "include/socket.h"
#include "include/io_uring.h"
#include "include/buffer_ring.h"
#include "../coroutine/include/task.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <cerrno>
#include <tuple>
#include <string>
#include <optional>

namespace co_uring {

// Constants
static constexpr int SOCKET_LISTEN_QUEUE_SIZE = 128;

// socket_client implementation

socket_client::recv_awaiter::recv_awaiter(std::uint32_t raw_fd) noexcept 
    : raw_fd_(raw_fd) {}

bool socket_client::recv_awaiter::await_ready() const noexcept { 
    return false; 
}

void socket_client::recv_awaiter::await_suspend(std::coroutine_handle<> coroutine) noexcept {
    sqe_data_.coroutine = coroutine.address();
    IoUring::getInstance().submitRecvRequest(&sqe_data_, raw_fd_);
}

int socket_client::recv_awaiter::await_resume() noexcept {
    if (sqe_data_.cqe_res < 0) {
        return sqe_data_.cqe_res;
    }
    
    buffer_id_ = sqe_data_.cqe_flags >> IORING_CQE_BUFFER_SHIFT;
    buffer_size_ = static_cast<std::uint32_t>(sqe_data_.cqe_res);
    return sqe_data_.cqe_res;
}

socket_client::recv_awaiter socket_client::recv() const noexcept {
    return recv_awaiter{get_raw_fd()};
}

socket_client::send_awaiter::send_awaiter(std::uint32_t raw_fd, 
                                          const std::uint8_t* buf, 
                                          std::size_t size) noexcept
    : raw_fd_(raw_fd), buffer_(buf), buffer_size_(size) {}

bool socket_client::send_awaiter::await_ready() const noexcept { 
    return false; 
}

void socket_client::send_awaiter::await_suspend(std::coroutine_handle<> coroutine) noexcept {
    sqe_data_.coroutine = coroutine.address();
    IoUring::getInstance().submitSendRequest(&sqe_data_, raw_fd_, buffer_, buffer_size_);
}

int socket_client::send_awaiter::await_resume() const noexcept {
    return sqe_data_.cqe_res;
}

auto socket_client::send(const std::uint8_t* buf, std::size_t size) const noexcept -> task<int> {
    std::uint32_t total_sent = 0;
    while (total_sent < size) {
        int result = co_await send_awaiter{get_raw_fd(), buf + total_sent, size - total_sent};
        if (result < 0) {
            co_return result;
        }
        total_sent += static_cast<std::uint32_t>(result);
    }
    co_return static_cast<int>(total_sent);
}

// socket_server implementation

int socket_server::listen(int backlog) const noexcept {
    return ::listen(get_raw_fd(), backlog);
}

socket_server::multishot_accept_guard::multishot_accept_guard(std::uint32_t raw_fd,
                                                              sockaddr_storage* client_address,
                                                              socklen_t* client_address_size) noexcept
    : initial_await_(true), raw_fd_(raw_fd), 
      client_address_(client_address), 
      client_address_size_(client_address_size) {}

socket_server::multishot_accept_guard::~multishot_accept_guard() noexcept {
    IoUring::getInstance().submitCancelRequest(&sqe_data_);
}

bool socket_server::multishot_accept_guard::await_ready() const noexcept { 
    return false; 
}

void socket_server::multishot_accept_guard::await_suspend(std::coroutine_handle<> coroutine) noexcept {
    sqe_data_.coroutine = coroutine.address();
    if (initial_await_) {
        IoUring::getInstance().submitMultishotAcceptRequest(
            &sqe_data_, raw_fd_, 
            reinterpret_cast<sockaddr*>(client_address_),
            client_address_size_);
        initial_await_ = false;
    }
}

socket_client* socket_server::multishot_accept_guard::await_resume() const noexcept {
    if (sqe_data_.cqe_res < 0) {
        return nullptr;
    }
    return new socket_client{static_cast<std::uint32_t>(sqe_data_.cqe_res)};
}

socket_server::multishot_accept_guard& 
socket_server::accept(sockaddr_storage* client_address, socklen_t* client_address_size) noexcept {
    if (!multishot_accept_guard_.has_value()) {
        multishot_accept_guard_.emplace(get_raw_fd(), client_address, client_address_size);
    }
    return *multishot_accept_guard_;
}

// Free function: bind
auto bind(const char* host, std::uint16_t port) noexcept -> std::optional<socket_server> {
    addrinfo address_hints;
    addrinfo* socket_address;
    std::memset(&address_hints, 0, sizeof(addrinfo));
    address_hints.ai_family = AF_UNSPEC;
    address_hints.ai_socktype = SOCK_STREAM;
    address_hints.ai_flags = AI_PASSIVE;

    const std::string port_str = std::to_string(port);
    int getaddrinfo_result = getaddrinfo(host, port_str.c_str(), &address_hints, &socket_address);
    if (getaddrinfo_result != 0) {
        return std::nullopt;
    }

    for (auto node = socket_address; node != nullptr; node = node->ai_next) { //if failed, try next address
        int sockfd = socket(node->ai_family, node->ai_socktype, node->ai_protocol);
        if (sockfd < 0) {
            continue;
        }

        const std::uint32_t flag = 1; //set socket options and bind socket to address and return socket_server
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0) {
            close(sockfd);
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &flag, sizeof(flag)) < 0) {
            close(sockfd);
            continue;
        }

        if (::bind(sockfd, node->ai_addr, node->ai_addrlen) < 0) {
            close(sockfd);
            continue;
        }

        freeaddrinfo(socket_address);
        return socket_server{static_cast<std::uint32_t>(sockfd)};
    } 
    freeaddrinfo(socket_address);
    return std::nullopt;
}

} // namespace co_uring