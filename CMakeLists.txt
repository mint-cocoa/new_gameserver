cmake_minimum_required(VERSION 3.20)
project(gameserver CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_library(URING_LIB uring REQUIRED)
if(NOT URING_LIB)
    message(FATAL_ERROR "liburing not found")
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR})

# Source files
set(IO_SOURCES
    io/buffer_ring.cpp
    io/io_uring.cpp
    io/socket.cpp
)

set(SESSION_SOURCES
    session/session.cpp
    session/service.cpp
)

set(SERVER_SOURCES
    server/server.cpp
)

set(ALL_SOURCES
    ${IO_SOURCES}
    ${SESSION_SOURCES}
    ${SERVER_SOURCES}
)

# Create static library for game server components
add_library(gameserver_lib STATIC ${ALL_SOURCES})
target_link_libraries(gameserver_lib ${URING_LIB})


# Main game server executable
add_executable(gameserver ${CMAKE_SOURCE_DIR}/main.cpp)
target_link_libraries(gameserver gameserver_lib pthread)




# Install targets
install(TARGETS gameserver
    DESTINATION bin
)