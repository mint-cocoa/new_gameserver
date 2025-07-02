#include "include/session.h"
#include "include/packet_handler.h"
#include "../io/include/buffer_ring.h"
#include "../io/include/logger.h"
#include "../coroutine/include/task.h"
#include <iostream>
#include <span>
#include <cstring>

namespace co_uring
{

    // Session base class implementation
    Session::Session(std::unique_ptr<socket_client> socket)
        : socket_(std::move(socket))
    {
        recv_buffer_.resize(RECV_BUFFER_SIZE);
    }

    Session::~Session() = default;

    task<void> Session::Send(std::vector<std::uint8_t>&& buffer)
    {
        if (!socket_)
        {
            LOG_ERROR("Attempted to send on a null socket");
            co_return;
        }

        auto send_result = co_await socket_->send({buffer.data(), buffer.size()});

        if (!send_result)
        {
            LOG_ERROR("💥 Session Send error: {}", static_cast<int>(send_result.error()));
        }
        else
        {
            LOG_DEBUG("✅ Session sent {} bytes", *send_result);
            OnSend(*send_result);
        }
    }

    // PacketSession implementation
    PacketSession::PacketSession(std::unique_ptr<socket_client> socket)
        : Session(std::move(socket))
    {}
    PacketSession::~PacketSession() = default;

    void PacketSession::OnRecv(const std::uint8_t *buffer, std::size_t len)
    {
        // Copy to internal buffer and process packets
        if (recv_buffer_pos_ + len > recv_buffer_.size())
        {
            recv_buffer_.resize(recv_buffer_pos_ + len);
        }

        std::memcpy(recv_buffer_.data() + recv_buffer_pos_, buffer, len);
        recv_buffer_pos_ += len;

        // Process complete packets
        std::size_t processed = ProcessPackets(recv_buffer_.data(), recv_buffer_pos_);

        // Move remaining data to front of buffer
        if (processed > 0 && processed < recv_buffer_pos_)
        {
            std::memmove(recv_buffer_.data(), recv_buffer_.data() + processed, recv_buffer_pos_ - processed);
            recv_buffer_pos_ -= processed;
        }
        else if (processed == recv_buffer_pos_)
        {
            recv_buffer_pos_ = 0;
        }
    }

    std::size_t PacketSession::ProcessPackets(const std::uint8_t *buffer, std::size_t len)
    {
        std::size_t processed = 0;

        while (processed + sizeof(PacketHeader) <= len)
        {
            const PacketHeader *header = reinterpret_cast<const PacketHeader *>(buffer + processed);

            if (processed + header->size <= len)
            {
                OnRecvPacket(buffer + processed, header->size);
                processed += header->size;
            }
            else
            {
                // Incomplete packet, wait for more data
                break;
            }
        }

        return processed;
    }

    // GameSession implementation
    GameSession::GameSession(std::unique_ptr<socket_client> socket)
        : PacketSession(std::move(socket))
    {
        std::cout << "GameSession created" << std::endl;
    }

    GameSession::~GameSession()
    {
        std::cout << "GameSession destroyed" << std::endl;
    }

    void GameSession::OnConnected()
    {
        std::cout << "Game client connected" << std::endl;

        // Send welcome packet or initial game state
        std::vector<std::uint8_t> welcomePacket;
        PacketHeader header{sizeof(PacketHeader), 1}; // Welcome packet ID = 1

        welcomePacket.resize(sizeof(PacketHeader));
        std::memcpy(welcomePacket.data(), &header, sizeof(PacketHeader));

        // TODO: Implement send functionality for welcome packet
        std::cout << "Welcome packet prepared (size: " << welcomePacket.size() << ")" << std::endl;
    }

    void GameSession::OnDisconnected()
    {
        std::cout << "Game client disconnected" << std::endl;

        // Clean up player state, notify other players, etc.
    }

    void GameSession::OnRecvPacket(const std::uint8_t *buffer, std::size_t len)
    {
        if (len < sizeof(PacketHeader))
        {
            std::cerr << "Invalid packet size: " << len << std::endl;
            return;
        }

        const PacketHeader *header = reinterpret_cast<const PacketHeader *>(buffer);

        std::cout << "Received packet - ID: " << header->id << ", Size: " << header->size << std::endl;

        // Pass to derived class for game-specific handling
        HandleGamePacket(header->id, buffer + sizeof(PacketHeader), len - sizeof(PacketHeader));
    }

    // GameSessionHandler implementation
    task<void> GameSessionHandler::handle_client(SessionRef session) noexcept
    {
        try
        {
            co_await process_session_loop(session);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error in handle_client: " << e.what() << std::endl;
        }
    }

    task<void> GameSessionHandler::process_session_loop(SessionRef session) noexcept
    {
        LOG_INFO("🔄 process_session_loop coroutine started - managing game session");
        auto &buffer_ring = BufferRing::getInstance();

        int loop_count = 0;

        while (true)
        {
            loop_count++;
            LOG_DEBUG("🔄 Session loop iteration {} - awaiting client data", loop_count);

            // Receive data using io_uring buffer selection
            LOG_DEBUG("⏳ Creating recv awaiter...");
            auto recv_awaiter = session->socket_->recv();
            LOG_DEBUG("⏸️ Suspending for recv operation...");
            int recv_result = co_await recv_awaiter;

            LOG_DEBUG("▶️ Resumed from recv - result: {}", recv_result);

            if (recv_result <= 0)
            {
                if (recv_result == 0)
                {
                    LOG_INFO("✅ Client disconnected gracefully");
                }
                else
                {
                    LOG_WARN("⚠️ Client disconnected with error: {}", recv_result);
                }
                session->OnDisconnected();
                break;
            }

            // Get buffer info from the awaiter
            std::uint32_t buf_id = recv_awaiter.get_buffer_id();
            std::uint32_t recv_buf_size = recv_awaiter.get_buffer_size();

            LOG_DEBUG("📥 Received {} bytes in buffer {}", recv_buf_size, buf_id);

            // Get the actual buffer data
            auto &buffer = buffer_ring.borrowBuf(buf_id);

            // Process received packets
            LOG_DEBUG("🎮 Processing packet data...");
            session->OnRecv(buffer.data(), recv_buf_size);

            // Immediately return the recv buffer to the pool
            buffer_ring.returnBuf(buf_id);
            LOG_DEBUG("🔄 Buffer {} returned to pool", buf_id);
        }

        LOG_INFO("🏁 Session coroutine ended after {} iterations", loop_count);
        co_return;
    }

    void GameSessionHandler::handle_packet(SessionRef session, const std::uint8_t *buffer, std::size_t len)
    {
        PacketHandler::handle_packet(session, buffer, len);
    }

} // namespace co_uring