#include "include/session.h"
#include "../io/include/buffer_ring.h"
#include "../coroutine/include/task.h"
#include <iostream>
#include <span>
#include <cstring>

namespace co_uring {

// Session base class implementation
Session::Session() {
    recv_buffer_.resize(RECV_BUFFER_SIZE);
}

Session::~Session() = default;










// PacketSession implementation
PacketSession::PacketSession() = default;
PacketSession::~PacketSession() = default;

void PacketSession::OnRecv(const std::uint8_t* buffer, std::size_t len) {
    // Copy to internal buffer and process packets
    if (recv_buffer_pos_ + len > recv_buffer_.size()) {
        recv_buffer_.resize(recv_buffer_pos_ + len);
    }
    
    std::memcpy(recv_buffer_.data() + recv_buffer_pos_, buffer, len);
    recv_buffer_pos_ += len;
    
    // Process complete packets
    std::size_t processed = ProcessPackets(recv_buffer_.data(), recv_buffer_pos_);
    
    // Move remaining data to front of buffer
    if (processed > 0 && processed < recv_buffer_pos_) {
        std::memmove(recv_buffer_.data(), recv_buffer_.data() + processed, recv_buffer_pos_ - processed);
        recv_buffer_pos_ -= processed;
    } else if (processed == recv_buffer_pos_) {
        recv_buffer_pos_ = 0;
    }
}

std::size_t PacketSession::ProcessPackets(const std::uint8_t* buffer, std::size_t len) {
    std::size_t processed = 0;
    
    while (processed + sizeof(PacketHeader) <= len) {
        const PacketHeader* header = reinterpret_cast<const PacketHeader*>(buffer + processed);
        
        if (processed + header->size <= len) {
            OnRecvPacket(buffer + processed, header->size);
            processed += header->size;
        } else {
            // Incomplete packet, wait for more data
            break;
        }
    }
    
    return processed;
}

// GameSession implementation
GameSession::GameSession() {
    std::cout << "GameSession created" << std::endl;
}

GameSession::~GameSession() {
    std::cout << "GameSession destroyed" << std::endl;
}

void GameSession::OnConnected() {
    std::cout << "Game client connected" << std::endl;
    
    // Send welcome packet or initial game state
    std::vector<std::uint8_t> welcomePacket;
    PacketHeader header{sizeof(PacketHeader), 1}; // Welcome packet ID = 1
    
    welcomePacket.resize(sizeof(PacketHeader));
    std::memcpy(welcomePacket.data(), &header, sizeof(PacketHeader));
    
    // TODO: Implement send functionality for welcome packet
    std::cout << "Welcome packet prepared (size: " << welcomePacket.size() << ")" << std::endl;
}

void GameSession::OnDisconnected() {
    std::cout << "Game client disconnected" << std::endl;
    
    // Clean up player state, notify other players, etc.
}

void GameSession::OnRecvPacket(const std::uint8_t* buffer, std::size_t len) {
    if (len < sizeof(PacketHeader)) {
        std::cerr << "Invalid packet size: " << len << std::endl;
        return;
    }
    
    const PacketHeader* header = reinterpret_cast<const PacketHeader*>(buffer);
    
    std::cout << "Received packet - ID: " << header->id << ", Size: " << header->size << std::endl;
    
    // Pass to derived class for game-specific handling
    HandleGamePacket(header->id, buffer + sizeof(PacketHeader), len - sizeof(PacketHeader));
}

// GameSessionHandler implementation
task<void> GameSessionHandler::handle_client(std::unique_ptr<socket_client> client) noexcept {
    try {
        co_await process_session_loop(std::move(client));
    } catch (const std::exception& e) {
        std::cerr << "Error in handle_client: " << e.what() << std::endl;
    }
}

task<void> GameSessionHandler::process_session_loop(std::unique_ptr<socket_client> client) noexcept {
    auto& buffer_ring = BufferRing::getInstance();
    
    while (true) {
        // Receive data using io_uring buffer selection
        auto recv_awaiter = client->recv();
        int recv_result = co_await recv_awaiter;
        if (recv_result <= 0) {
            std::cout << "Client disconnected" << std::endl;
            break;
        }
        
        // Get buffer info from the awaiter
        std::uint32_t buf_id = recv_awaiter.get_buffer_id();
        std::uint32_t recv_buf_size = recv_awaiter.get_buffer_size();
        
        // Get the actual buffer data
        auto& buffer = buffer_ring.borrowBuf(buf_id);
        
        // Process received packets using span (no data copy)
        handle_packet(buffer.data(), recv_buf_size);
        
        // Echo back for testing (remove in actual game)
        int send_result = co_await client->send(buffer.data(), recv_buf_size);
        if (send_result < 0) {
            std::cerr << "Send error: " << send_result << std::endl;
            buffer_ring.returnBuf(buf_id);
            break;
        }
        
        // Return buffer to pool for reuse
        buffer_ring.returnBuf(buf_id);
    }
    
    std::cout << "Session ended" << std::endl;
    co_return;
}

void GameSessionHandler::handle_packet(const std::uint8_t* buffer, std::size_t len) {
    if (len < sizeof(PacketHeader)) {
        std::cerr << "Invalid packet size: " << len << std::endl;
        return;
    }
    
    const PacketHeader* header = reinterpret_cast<const PacketHeader*>(buffer);
    
    std::cout << "Processing packet - ID: " << header->id 
              << ", Size: " << header->size 
              << ", Data len: " << len << std::endl;
    
    // Process different packet types
    switch (header->id) {
        case 1: // Welcome response
            std::cout << "Received welcome response" << std::endl;
            break;
        case 2: // Player move
            std::cout << "Received player move" << std::endl;
            break;
        case 3: // Chat message
            std::cout << "Received chat message" << std::endl;
            break;
        default:
            std::cout << "Unknown packet type: " << header->id << std::endl;
            break;
    }
}

} // namespace co_uring