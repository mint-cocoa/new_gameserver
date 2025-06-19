#include "include/buffer_ring.h"
#include "include/io_uring.h"
#include <sys/mman.h>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <mutex>
#include <iostream>
#include <liburing.h>
#include <ranges>
#include <unistd.h>

namespace co_uring {

int BufferRing::registerBufRing() noexcept {
    const std::size_t page_size = sysconf(_SC_PAGESIZE);
    const std::size_t size = BUF_RING_SIZE * sizeof(io_uring_buf);
    const std::size_t aligned_size = (size + page_size - 1) & ~(page_size - 1);
    
    void *buf_ring = mmap(nullptr, aligned_size, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (buf_ring == MAP_FAILED) {
        return -errno;
    }

    buf_ring_ = reinterpret_cast<io_uring_buf_ring *>(buf_ring);

    buf_list_.reserve(BUF_RING_SIZE);
    for (unsigned i = 0; i < BUF_RING_SIZE; ++i) {
        buf_list_.emplace_back(BUF_SIZE);
    }

    return IoUring::getInstance().setupBufRing(buf_ring_, buf_list_);
}

std::vector<std::uint8_t>& BufferRing::borrowBuf(const std::uint32_t buf_id) noexcept {
    borrowed_buf_set_[buf_id] = true;
    return buf_list_[buf_id];
}

void BufferRing::returnBuf(const std::uint32_t buf_id) noexcept {
    borrowed_buf_set_[buf_id] = false;
    IoUring::getInstance().addBuf(buf_ring_, buf_list_[buf_id].data(), buf_list_[buf_id].size(), buf_id);
}

BufferRing::~BufferRing() {
    try {
        if (buf_ring_) {
            const size_t page_size = sysconf(_SC_PAGESIZE);
            const size_t size = BUF_RING_SIZE * sizeof(io_uring_buf);
            const size_t aligned_size = (size + page_size - 1) & ~(page_size - 1);
            munmap(buf_ring_, aligned_size);
            buf_ring_ = nullptr;
            std::cout << "BufferRing destroyed successfully" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error during BufferRing destruction: " << e.what() << std::endl;
    }
}

} // namespace co_uring