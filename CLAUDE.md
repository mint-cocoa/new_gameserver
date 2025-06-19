# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a high-performance game server built with modern C++ using io_uring for asynchronous I/O and C++20 coroutines for concurrency. The server is designed to handle thousands of concurrent connections with minimal latency.

## Build Commands

```bash
# Create build directory
mkdir -p build && cd build

# Configure with CMake
cmake ..

# Build the project
make -j$(nproc)

# Run the server (requires root for io_uring or appropriate capabilities)
./gameserver
```

## Architecture

### Threading Model
- Multi-worker architecture with thread-local io_uring instances
- Default: 4 worker threads (configurable in main.cpp:26)
- Each worker binds to port 8080 with SO_REUSEPORT for kernel-level load balancing
- Lock-free within worker threads

### Core Components

#### Coroutine Infrastructure (`coroutine/include/`)
- `task<T>`: Awaitable coroutine type with value/exception propagation
- `simple_task`: Lightweight void-returning coroutine
- Fire-and-forget execution with `spawn()` helper

#### io_uring Integration (`io/include/io_uring.h`)
- Thread-local singleton pattern for `IoUring` instances
- `sqe_data`: Links io_uring operations to coroutine handles
- Event loop in `IoUring::run()` processes completions and resumes coroutines

#### Socket Layer (`io/include/socket.h`)
- `socket_server`: Multishot accept for handling multiple connections per syscall
- `socket_client`: Coroutine-aware recv/send operations
- Custom awaitables integrate with io_uring for async I/O

#### Buffer Management (`io/include/buffer_ring.h`)
- Thread-local `BufferRing` with 256 × 4KB buffers
- Zero-copy I/O using io_uring buffer selection
- RAII buffer borrowing with automatic return

#### Session Management (`session/include/`)
- `Session`: Base connection management
- `PacketSession`: Packet framing with 16-bit size + 16-bit ID header
- `GameSession`: Game-specific logic implementation
- `GameSessionHandler`: Coroutine processing client connections

### Key Design Patterns
- **Thread-local singletons**: Avoid synchronization for io_uring/buffers
- **RAII everywhere**: Automatic cleanup for all resources
- **Coroutine-per-connection**: Natural async flow for game logic
- **Zero-copy networking**: Buffer selection minimizes memory copies

## Important Constants
- Server port: 8080 (main.cpp:30)
- Worker threads: 4 (main.cpp:26)
- io_uring queue size: 4096 (io/include/io_uring.h)
- Buffer size: 4KB, Buffer count: 256 (io/include/buffer_ring.h)
- Buffer group ID: 1

## Error Handling
- Functions return negative errno values on failure
- Coroutines propagate exceptions through task<T>
- RAII ensures cleanup even on errors

## Requirements
- Linux kernel 5.1+ with io_uring support
- liburing development libraries
- C++20 compiler (GCC 10+ or Clang 12+)
- CMake 3.20+