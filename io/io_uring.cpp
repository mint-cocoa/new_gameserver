#include "include/io_uring.h"
#include "include/buffer_ring.h"
#include "include/logger.h"
#include <iostream>
#include <coroutine>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <unistd.h>

namespace co_uring
{

    IoUring &IoUring::getInstance()
    {
        thread_local IoUring instance;
        return instance;
    }

    IoUring::~IoUring() { io_uring_queue_exit(&io_uring_); }

    int IoUring::queueInit()
    {
        int result = io_uring_queue_init(IO_URING_QUEUE_SIZE, &io_uring_, 0);
        if (result < 0)
        {
            std::cerr << "Failed to initialize io_uring queue: " << strerror(-result) << std::endl;
            return result;
        }

        std::cout << "IoUring queue initialized with size: " << IO_URING_QUEUE_SIZE << std::endl;
        return 0;
    }

    void IoUring::eventLoop()
    {
        LOG_INFO("🔄 IoUring::eventLoop starting - managing coroutine lifecycle");

        while (true)
        {
            LOG_DEBUG("⏳ Waiting for io_uring events...");
            auto result = submitAndWait(1);
            if (!result)
            {
                LOG_ERROR("❌ Failed to submit and wait: {}", result);
                continue;
            }
            LOG_DEBUG("📥 Received {} events", result);

            std::uint32_t head = 0;
            io_uring_cqe *cqe = nullptr;
            int cqe_count = 0;

            io_uring_for_each_cqe(&io_uring_, head, cqe)
            {
                cqe_count++;
                auto *sqe_data_ptr = reinterpret_cast<sqe_data *>(io_uring_cqe_get_data(cqe));

                if (sqe_data_ptr)
                {
                    sqe_data_ptr->cqe_res = cqe->res;
                    sqe_data_ptr->cqe_flags = cqe->flags;

                    LOG_DEBUG("🔍 Processing CQE #{} - res: {}, flags: 0x{:08x}", cqe_count, cqe->res, cqe->flags);

                    void *coroutine_address = sqe_data_ptr->coroutine;
                    io_uring_cqe_seen(&io_uring_, cqe);

                    if (coroutine_address != nullptr)
                    {
                        LOG_DEBUG("▶️ Resuming coroutine - handle: 0x{:016x}", reinterpret_cast<uintptr_t>(coroutine_address));

                        try
                        {
                            auto handle = std::coroutine_handle<>::from_address(coroutine_address);

                            if (handle)
                            {
                                if (!handle.done())
                                {
                                    LOG_DEBUG("🔄 Coroutine is alive, resuming...");
                                    handle.resume();
                                    LOG_DEBUG("✅ Coroutine resumed successfully");
                                }
                                else
                                {
                                    LOG_WARN("⚠️ Coroutine already done, skipping resume");
                                }
                            }
                            else
                            {
                                LOG_ERROR("❌ Invalid coroutine handle from address: 0x{:016x}", reinterpret_cast<uintptr_t>(coroutine_address));
                            }
                        }
                        catch (const std::exception &e)
                        {
                            LOG_ERROR("💥 Exception during coroutine resume: {}", e.what());
                        }
                        catch (...)
                        {
                            LOG_ERROR("💥 Unknown exception during coroutine resume");
                        }
                    }
                    else
                    {
                        LOG_WARN("⚠️ CQE has null coroutine address - orphaned operation");
                    }
                }
                else
                {
                    LOG_WARN("⚠️ CQE has null sqe_data - invalid operation");
                    io_uring_cqe_seen(&io_uring_, cqe);
                }
            }

            if (cqe_count > 0)
            {
                LOG_DEBUG("📊 Processed {} CQEs in this batch", cqe_count);
            }
        }
    }

    int IoUring::submitAndWait(std::uint32_t wait_nr)
    {
        int result = io_uring_submit_and_wait(&io_uring_, wait_nr);
        if (result < 0)
        {
            std::cerr << "Failed to submit and wait: " << strerror(-result) << std::endl;
            return result;
        }

        return result;
    }

    void IoUring::submitMultishotAcceptRequest(sqe_data *sqe_data_ptr, std::uint32_t raw_fd,
                                               sockaddr *client_addr, socklen_t *client_len)
    {
        io_uring_sqe *sqe = io_uring_get_sqe(&io_uring_);
        if (!sqe)
        {
            std::cerr << "Failed to get SQE for multishot accept" << std::endl;
            return;
        }

        io_uring_prep_multishot_accept(sqe, raw_fd, client_addr, client_len, 0);
        io_uring_sqe_set_data(sqe, sqe_data_ptr);

        // Submitted multishot accept request
    }

    void IoUring::submitRecvRequest(sqe_data *sqe_data_ptr, std::uint32_t raw_fd)
    {
        io_uring_sqe *sqe = io_uring_get_sqe(&io_uring_);
        if (!sqe)
        {
            std::cerr << "Failed to get SQE for recv" << std::endl;
            return;
        }

        io_uring_prep_recv(sqe, raw_fd, nullptr, BUF_SIZE, 0);
        io_uring_sqe_set_flags(sqe, IOSQE_BUFFER_SELECT);
        io_uring_sqe_set_data(sqe, sqe_data_ptr);
        sqe->buf_group = BUF_GROUP_ID;

        // Submitted recv request
    }

    void IoUring::submitSendRequest(sqe_data *sqe_data_ptr, std::uint32_t raw_fd, std::span<const std::uint8_t> buf)
    {
        io_uring_sqe *sqe = io_uring_get_sqe(&io_uring_);
        if (!sqe)
        {
            std::cerr << "Failed to get SQE for send" << std::endl;
            return;
        }

        io_uring_prep_send(sqe, raw_fd, buf.data(), buf.size(), 0);
        io_uring_sqe_set_data(sqe, sqe_data_ptr);

        // Submitted send request
    }

    void IoUring::submitSpliceRequest(sqe_data *sqe_data_ptr, std::uint32_t raw_fd_in,
                                      std::uint32_t raw_fd_out, std::uint32_t len)
    {
        io_uring_sqe *sqe = io_uring_get_sqe(&io_uring_);
        if (!sqe)
        {
            std::cerr << "Failed to get SQE for splice" << std::endl;
            return;
        }

        io_uring_prep_splice(sqe, raw_fd_in, -1, raw_fd_out, -1, len, SPLICE_F_MORE);
        io_uring_sqe_set_data(sqe, sqe_data_ptr);

        // Submitted splice request
    }

    void IoUring::submitCancelRequest(sqe_data *sqe_data_ptr)
    {
        io_uring_sqe *sqe = io_uring_get_sqe(&io_uring_);
        if (!sqe)
        {
            std::cerr << "Failed to get SQE for cancel" << std::endl;
            return;
        }

        io_uring_prep_cancel(sqe, sqe_data_ptr, 0);

        // Submitted cancel request
    }

    void IoUring::addBuf(io_uring_buf_ring *buf_ring,
                         std::uint8_t *buf, std::size_t buf_size,
                         std::uint32_t buf_id)
    {
        if (!buf_ring)
        {
            std::cerr << "Null buffer ring pointer" << std::endl;
            return;
        }

        if (!buf || buf_size == 0)
        {
            std::cerr << "Invalid buffer" << std::endl;
            return;
        }

        const std::uint32_t mask = io_uring_buf_ring_mask(BUF_RING_SIZE);
        io_uring_buf_ring_add(buf_ring, buf, buf_size, buf_id, mask, buf_id);
        io_uring_buf_ring_advance(buf_ring, 1);

        // Added buffer to ring
    }

    int IoUring::setupBufRing(io_uring_buf_ring *buf_ring,
                              const std::vector<std::vector<std::uint_least8_t>> &buf_list) noexcept
    {
        try
        {
            // Initialize buffer ring structure
            io_uring_buf_ring_init(buf_ring);

            // Register buffer ring with kernel
            io_uring_buf_reg buf_reg = {};
            buf_reg.ring_addr = reinterpret_cast<__u64>(buf_ring);
            buf_reg.ring_entries = BUF_RING_SIZE;
            buf_reg.bgid = BUF_GROUP_ID;

            int result = io_uring_register_buf_ring(&io_uring_, &buf_reg, 0);
            if (result < 0)
            {
                std::cerr << "Failed to register buffer ring: " << strerror(-result) << std::endl;
                return result;
            }

            // Add all buffers to the ring
            for (std::uint32_t i = 0; i < buf_list.size(); ++i)
            {
                io_uring_buf_ring_add(buf_ring,
                                      const_cast<std::uint8_t *>(buf_list[i].data()),
                                      buf_list[i].size(),
                                      i,
                                      io_uring_buf_ring_mask(buf_list.size()),
                                      i);
            }

            // Submit all buffers at once
            io_uring_buf_ring_advance(buf_ring, buf_list.size());

            std::cout << "Buffer ring registered successfully with " << buf_list.size() << " buffers" << std::endl;

            return 0;
        }
        catch (...)
        {
            return -ENOMEM;
        }
    }

    int IoUring::decodeVoid(int result)
    {
        return result;
    }

    int IoUring::decode(int result)
    {
        return result;
    }

    void IoUring::unwrap(int result)
    {
        if (result < 0)
        {
            std::cerr << "Unwrap failed with error: " << result << std::endl;
        }
    }

} // namespace co_uring