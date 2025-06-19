#pragma once

#include "session.h"
#include "../../io/include/socket.h"
#include <memory>
#include <set>
#include <atomic>
#include <mutex>
#include <string>

namespace co_uring {

enum class ServiceType {
    Server,
    Client
};

struct NetAddress {
    std::string host;
    std::uint16_t port;
    
    NetAddress(const std::string& h, std::uint16_t p) : host(h), port(p) {}
};

class Service : public std::enable_shared_from_this<Service> {
public:
    Service(ServiceType type, NetAddress address, SessionFactory factory, std::int32_t maxSessionCount);
    virtual ~Service();
    
    virtual bool Start() = 0;
    virtual void CloseService();
    
    void Broadcast(const std::uint8_t* data, std::size_t size);
    void Broadcast(const std::vector<std::uint8_t>& data);
    
    SessionRef CreateSession();
    void AddSession(SessionRef session);
    void ReleaseSession(SessionRef session);
    
    ServiceType GetServiceType() const { return type_; }
    const NetAddress& GetNetAddress() const { return netAddress_; }
    std::int32_t GetMaxSessionCount() const { return maxSessionCount_; }
    std::int32_t GetSessionCount() const { return sessionCount_.load(); }
    
protected:
    bool CanStart() const { return sessionFactory_ != nullptr; }

private:
    ServiceType type_;
    NetAddress netAddress_;
    SessionFactory sessionFactory_;
    std::int32_t maxSessionCount_;
    
    std::atomic<std::int32_t> sessionCount_{0};
    std::set<SessionRef> sessions_;
    std::mutex sessionMutex_;
};

class ServerService : public Service {
public:
    ServerService(NetAddress address, SessionFactory factory, std::int32_t maxSessionCount);
    
    bool Start() override;
    void CloseService() override;
    
private:
    std::unique_ptr<socket_server> listener_;
};

class ClientService : public Service {
public:
    ClientService(NetAddress targetAddress, SessionFactory factory, std::int32_t maxSessionCount);
    
    bool Start() override;
};

using ServiceRef = std::shared_ptr<Service>;
using ServerServiceRef = std::shared_ptr<ServerService>;
using ClientServiceRef = std::shared_ptr<ClientService>;

} // namespace co_uring