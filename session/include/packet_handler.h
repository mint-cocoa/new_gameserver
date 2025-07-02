#pragma once

#include "session.h"
#include <cstdint>
#include <cstddef>

namespace co_uring {

class PacketHandler {
public:
    static void handle_packet(SessionRef session, const std::uint8_t* buffer, std::size_t len);
};

} // namespace co_uring
