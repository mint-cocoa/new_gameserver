#pragma once

#include <liburing.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <cstdint>
#include <memory>
#include <vector>
#include <cerrno>
#include <span>

namespace co_uring
{

    class sqe_data
    {
    public:
        void *coroutine = nullptr;
        std::int32_t cqe_res = 0;
        std::uint32_t cqe_flags = 0;
    };

    class IoUring
    {
    public:
        static constexpr std::uint32_t IO_URING_QUEUE_SIZE = 4096;
        static constexpr std::uint32_t BUF_RING_SIZE = 1024;
        static constexpr std::uint32_t BUF_SIZE = 8192;
        static constexpr std::uint32_t BUF_GROUP_ID = 1;

        static IoUring &getInstance();

        ~IoUring();

        int queueInit();

        void eventLoop();

        int registerBufRing();

        // Setup buffer ring with provided buffer ring pointer and buffer list
        int setupBufRing(io_uring_buf_ring *buf_ring,
                         const std::vector<std::vector<std::uint_least8_t>> &buf_list) noexcept;

        int submitAndWait(std::uint32_t wait_nr);

        void submitMultishotAcceptRequest(sqe_data *sqe_data_ptr, std::uint32_t raw_fd,
                                          sockaddr *client_addr, socklen_t *client_len);

        void submitRecvRequest(sqe_data *sqe_data_ptr, std::uint32_t raw_fd);

        void submitSendRequest(sqe_data *sqe_data_ptr, std::uint32_t raw_fd, std::span<const std::uint8_t> buf);

        void submitSpliceRequest(sqe_data *sqe_data_ptr, std::uint32_t raw_fd_in,
                                 std::uint32_t raw_fd_out, std::uint32_t len);

        void submitCancelRequest(sqe_data *sqe_data_ptr);

        void addBuf(io_uring_buf_ring *buf_ring,
                    std::uint8_t *buf, std::size_t buf_size,
                    std::uint32_t buf_id);

    private:
        IoUring() = default;
        IoUring(const IoUring &) = delete;
        IoUring &operator=(const IoUring &) = delete;

        int decodeVoid(int result);
        int decode(int result);
        void unwrap(int result);

        io_uring io_uring_;
    };

} // namespace co_uring