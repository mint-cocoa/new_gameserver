#include "include/packet_handler.h"
#include "include/session.h"
#include <iostream>
#include <vector>
#include <string>

namespace co_uring {

void PacketHandler::handle_packet(SessionRef session, const std::uint8_t* buffer, std::size_t len) {
    if (len < sizeof(PacketHeader)) {
        std::cerr << "Invalid packet size: " << len << std::endl;
        return;
    }

    const PacketHeader* header = reinterpret_cast<const PacketHeader*>(buffer);
    const std::uint8_t* data = buffer + sizeof(PacketHeader);
    const std::size_t data_len = len - sizeof(PacketHeader);

    std::cout << "[PacketHandler] Processing packet - ID: " << header->id
              << ", Size: " << header->size << std::endl;

    // Process different packet types
    switch (header->id) {
        case 1: // Welcome response
            std::cout << "[PacketHandler] Received welcome response" << std::endl;
            break;
        case 2: // Player move
            std::cout << "[PacketHandler] Received player move" << std::endl;
            break;
        case 3: { // Chat message
            std::string chat_msg(reinterpret_cast<const char*>(data), data_len);
            std::cout << "[PacketHandler] Received chat message: " << chat_msg << std::endl;

            // Create a response packet
            std::string response_msg = "[Server echo]: " + chat_msg;
            std::vector<std::uint8_t> response_packet(sizeof(PacketHeader) + response_msg.length());
            PacketHeader response_header{static_cast<uint16_t>(response_packet.size()), 103};
            
            memcpy(response_packet.data(), &response_header, sizeof(PacketHeader));
            memcpy(response_packet.data() + sizeof(PacketHeader), response_msg.data(), response_msg.length());

            // Send the response
            session->Send(std::move(response_packet));
            break;
        }
        default:
            std::cout << "[PacketHandler] Unknown packet type: " << header->id << std::endl;
            break;
    }
}

} // namespace co_uring
