#pragma once

#include <bitset>
#include <vector>
#include <liburing.h>
#include <cstdint>
#include <memory>

namespace co_uring {

class IoUring;

class BufferRing {
public:
    static constexpr unsigned IO_BUFFER_SIZE = 4096;
    static constexpr unsigned NUM_IO_BUFFERS = 256;
    static constexpr unsigned BUF_RING_SIZE = 256;
    static constexpr unsigned BUF_SIZE = 4096;

    BufferRing() = default;
    ~BufferRing();

    // Thread-local singleton access
    static auto getInstance() noexcept -> BufferRing& {
        thread_local BufferRing instance;
        return instance;
    }

    // Initialize buffer ring (call once)
    int registerBufRing() noexcept;

    // Delete copy operations
    BufferRing(const BufferRing&) = delete;
    BufferRing& operator=(const BufferRing&) = delete;

    // Borrow buffer by ID
    std::vector<std::uint8_t>& borrowBuf(const std::uint32_t buf_id) noexcept;

    // Return buffer by ID
    void returnBuf(const std::uint32_t buf_id) noexcept;

    // Check if initialized
    auto isInitialized() const noexcept -> bool { return buf_ring_ != nullptr; }

private:
    auto decodeVoid(int result) noexcept -> int;

    std::bitset<BUF_RING_SIZE> borrowed_buf_set_;
    std::vector<std::vector<std::uint8_t>> buf_list_;
    io_uring_buf_ring* buf_ring_{nullptr};
};

} // namespace co_uring