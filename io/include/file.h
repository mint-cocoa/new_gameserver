#pragma once

#include <optional>
#include <cstdint>
#include <unistd.h>
#include <utility>

namespace co_uring {

class file {
public:
    explicit file(const std::uint32_t raw_fd) noexcept : raw_fd_{raw_fd} {}

    file(file&& other) noexcept 
        : raw_fd_{std::exchange(other.raw_fd_, std::nullopt)} {}

    file(const file&) = delete;
    file& operator=(const file&) = delete;
    file& operator=(file&&) = delete;

    virtual ~file() noexcept {
        if (raw_fd_.has_value()) {
            ::close(*raw_fd_);
        }
    }

    [[nodiscard]] std::uint32_t get_raw_fd() const noexcept {
        return *raw_fd_;
    }

    [[nodiscard]] bool is_valid() const noexcept {
        return raw_fd_.has_value();
    }

protected:
    std::optional<std::uint32_t> raw_fd_;
};

} // namespace co_uring