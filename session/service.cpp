#include "include/service.h"
#include <iostream>
#include <algorithm>

namespace co_uring {

Service::Service(ServiceType type, NetAddress address, SessionFactory factory, std::int32_t maxSessionCount)
    : type_(type), netAddress_(address), sessionFactory_(factory), maxSessionCount_(maxSessionCount) {
}

Service::~Service() {
}

void Service::CloseService() {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    
    for (auto& session : sessions_) {
        // TODO: Implement proper disconnect method
        std::cout << "Session disconnecting (service closing)" << std::endl;
    }
    
    sessions_.clear();
    sessionCount_.store(0);
}

void Service::Broadcast(const std::uint8_t* data, std::size_t size) {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    
    for (const auto& session : sessions_) {
        // TODO: Implement proper send method
        std::cout << "Broadcasting data to session (size: " << size << ")" << std::endl;
    }
}

void Service::Broadcast(const std::vector<std::uint8_t>& data) {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    
    for (const auto& session : sessions_) {
        // TODO: Implement proper send method
        std::cout << "Broadcasting data to session (size: " << data.size() << ")" << std::endl;
    }
}

SessionRef Service::CreateSession() {
    SessionRef session = sessionFactory_();
    session->SetService(shared_from_this());
    return session;
}

void Service::AddSession(SessionRef session) {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    
    sessionCount_++;
    sessions_.insert(session);
    
    std::cout << "Session added. Total sessions: " << sessionCount_.load() << std::endl;
}

void Service::ReleaseSession(SessionRef session) {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    
    auto it = sessions_.find(session);
    if (it != sessions_.end()) {
        sessions_.erase(it);
        sessionCount_--;
        std::cout << "Session released. Total sessions: " << sessionCount_.load() << std::endl;
    }
}

// ServerService implementation
ServerService::ServerService(NetAddress address, SessionFactory factory, std::int32_t maxSessionCount)
    : Service(ServiceType::Server, address, factory, maxSessionCount) {
}

bool ServerService::Start() {
    if (!CanStart()) {
        return false;
    }
    
    // TODO: Implement socket_server::create method
    // listener_ = socket_server::create(GetNetAddress().host.c_str(), GetNetAddress().port);
    // if (!listener_) {
    //     std::cerr << "Failed to create listener socket" << std::endl;
    //     return false;
    // }
    // 
    // if (listener_->bind_and_listen() != 0) {
    //     std::cerr << "Failed to bind and listen" << std::endl;
    //     return false;
    // }
    
    std::cout << "Server started on " << GetNetAddress().host << ":" << GetNetAddress().port << std::endl;
    
    // TODO: Start accepting connections in coroutine
    // For now, this is a placeholder
    
    return true;
}

void ServerService::CloseService() {
    listener_.reset();
    Service::CloseService();
}

// ClientService implementation
ClientService::ClientService(NetAddress targetAddress, SessionFactory factory, std::int32_t maxSessionCount)
    : Service(ServiceType::Client, targetAddress, factory, maxSessionCount) {
}

bool ClientService::Start() {
    if (!CanStart()) {
        return false;
    }
    
    const std::int32_t sessionCount = GetMaxSessionCount();
    for (std::int32_t i = 0; i < sessionCount; i++) {
        SessionRef session = CreateSession();
        // TODO: Implement session Connect method
        // if (!session->Connect(GetNetAddress().host.c_str(), GetNetAddress().port)) {
        //     std::cerr << "Failed to connect session " << i << std::endl;
        //     return false;
        // }
        
        AddSession(session);
    }
    
    std::cout << "Client service started with " << sessionCount << " connections" << std::endl;
    return true;
}

} // namespace co_uring