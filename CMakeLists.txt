cmake_minimum_required(VERSION 3.20)
project(gameserver CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate compile_commands.json for better IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find required packages
find_library(URING_LIB uring REQUIRED)
if(NOT URING_LIB)
    message(FATAL_ERROR "liburing not found")
endif()

# Find clang-tidy and cppcheck
find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
find_program(CPPCHECK_EXE NAMES "cppcheck")

# Include directories
include_directories(${CMAKE_SOURCE_DIR})

# Source files
set(IO_SOURCES
    io/buffer_ring.cpp
    io/io_uring.cpp
    io/socket.cpp
    io/logger.cpp
)

set(SESSION_SOURCES
    session/session.cpp
    session/service.cpp
    session/packet_handler.cpp
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

# Set compiler flags for better error detection
target_compile_options(gameserver_lib PRIVATE 
    -Wall 
    -Wextra 
    -Wpedantic 
    -Werror=return-type
    -Werror=unused-variable
    -Werror=unused-function
    -g
    -O0
)

# Main game server executable
add_executable(gameserver ${CMAKE_SOURCE_DIR}/main.cpp)
target_link_libraries(gameserver gameserver_lib pthread)

# Set compiler flags for main executable
target_compile_options(gameserver PRIVATE 
    -Wall 
    -Wextra 
    -Wpedantic 
    -Werror=return-type
    -Werror=unused-variable
    -Werror=unused-function
    -g
    -O0
)

# clang-tidy target
if(CLANG_TIDY_EXE)
    add_custom_target(clang-tidy
        COMMAND ${CLANG_TIDY_EXE}
        -p=${CMAKE_BINARY_DIR}
        -checks=performance-*,modernize-*,bugprone-*,cert-*,cppcoreguidelines-*,readability-*,portability-*,misc-*
        -header-filter=".*"
        ${CMAKE_SOURCE_DIR}/io/buffer_ring.cpp
        ${CMAKE_SOURCE_DIR}/io/io_uring.cpp
        ${CMAKE_SOURCE_DIR}/io/socket.cpp
        ${CMAKE_SOURCE_DIR}/session/session.cpp
        ${CMAKE_SOURCE_DIR}/session/service.cpp
        ${CMAKE_SOURCE_DIR}/server/server.cpp
        ${CMAKE_SOURCE_DIR}/main.cpp
        COMMENT "Running clang-tidy..."
    )
endif()

# cppcheck target
if(CPPCHECK_EXE)
    add_custom_target(cppcheck
        COMMAND ${CPPCHECK_EXE}
        --enable=all
        --std=c++20
        --language=c++
        --template=gcc
        --inline-suppr
        --suppress=missingIncludeSystem
        --suppress=unusedFunction
        -I${CMAKE_SOURCE_DIR}
        -I${CMAKE_SOURCE_DIR}/coroutine/include
        -I${CMAKE_SOURCE_DIR}/io/include
        -I${CMAKE_SOURCE_DIR}/server/include
        -I${CMAKE_SOURCE_DIR}/session/include
        ${ALL_SOURCES}
        ${CMAKE_SOURCE_DIR}/main.cpp
        COMMENT "Running cppcheck..."
    )
endif()

# Install targets
install(TARGETS gameserver
    DESTINATION bin
)