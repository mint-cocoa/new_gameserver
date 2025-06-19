#pragma once

#include "../../io/include/socket.h"
#include "../../io/include/buffer_ring.h"
#include "../../coroutine/include/task.h"
#include <memory>
#include <atomic>
#include <queue>
#include <coroutine>
#include <vector>
#include <cstdint>
#include <functional>

namespace co_uring {

struct PacketHeader {
    std::uint16_t size;
    std::uint16_t id;
};

class Service;

class Session : public std::enable_shared_from_this<Session> {
public:
    Session();
    virtual ~Session();
    
    void SetService(std::shared_ptr<Service> service) { service_ = service; }
    std::shared_ptr<Service> GetService() const { return service_.lock(); }
    
    virtual void OnConnected() {}
    virtual void OnDisconnected() {}
    virtual void OnRecv(const std::uint8_t* buffer, std::size_t len) {}
    virtual void OnSend(std::size_t numOfBytes) {}

protected:
    std::unique_ptr<socket_client> socket_;
    std::atomic<bool> connected_{false};
    std::weak_ptr<Service> service_;
    
    // Receive buffer management
    static constexpr std::size_t RECV_BUFFER_SIZE = 4096;
    std::vector<std::uint8_t> recv_buffer_;
    std::size_t recv_buffer_pos_{0};
    
    // Send queue
    std::queue<std::vector<std::uint8_t>> send_queue_;
    std::atomic<bool> send_registered_{false};
    
private:
    friend class Service;
    void ProcessRecv();
    void ProcessSend();
};

class PacketSession : public Session {
public:
    PacketSession();
    virtual ~PacketSession();

protected:
    void OnRecv(const std::uint8_t* buffer, std::size_t len) override;
    virtual void OnRecvPacket(const std::uint8_t* buffer, std::size_t len) {}
    
private:
    std::size_t ProcessPackets(const std::uint8_t* buffer, std::size_t len);
};

using SessionRef = std::shared_ptr<Session>;
using SessionFactory = std::function<SessionRef()>;

// Game server session handler coroutine
class GameSessionHandler {
public:
    static task<void> handle_client(std::unique_ptr<socket_client> client) noexcept;

private:
    static task<void> process_session_loop(std::unique_ptr<socket_client> client) noexcept;
    static void handle_packet(const std::uint8_t* buffer, std::size_t len);
};

class GameSession : public PacketSession {
public:
    GameSession();
    virtual ~GameSession();

protected:
    void OnConnected() override;
    void OnDisconnected() override;
    void OnRecvPacket(const std::uint8_t* buffer, std::size_t len) override;
    
    virtual void HandleGamePacket(std::uint16_t packetId, const std::uint8_t* data, std::size_t len) {}
};

} // namespace co_uring