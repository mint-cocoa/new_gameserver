# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a high-performance game server built on modern C++ with io_uring and C++20 coroutines. The project implements asynchronous I/O operations using Linux's io_uring interface with coroutine-based concurrency.

## Architecture

### Core Components

- **io_uring Integration**: Central async I/O system using liburing
  - `IoUring` class: Thread-local singleton managing io_uring queues
  - `BufferRing` class: Singleton managing shared memory buffers for zero-copy I/O
  - `sqe_data` struct: Links io_uring operations to coroutine handles

- **Socket Layer**: Coroutine-aware networking built on io_uring
  - `socket_server`: Handles multishot accept operations
  - `socket_client`: Manages client connections with async recv/send
  - Both inherit from `file` base class (currently missing)

- **Buffer Management**: Optimized memory management
  - Fixed-size buffer ring (256 buffers × 4KB each)
  - Zero-copy operations using buffer selection
  - Automatic buffer recycling

### Missing Dependencies

The codebase currently has incomplete coroutine infrastructure:
- `coroutine/include/task.h` - Core coroutine task type
- `io/include/file.h` - Base class for file descriptors

## Key Constants

- IO_URING_QUEUE_SIZE: 4096
- Buffer size: 4KB per buffer
- Buffer count: 256 buffers
- Default server port: 8080
- Buffer group ID: 1

## Development Notes

- Uses C++20 coroutines with custom awaitable types
- Thread-local io_uring instances for thread safety
- Singleton pattern for buffer ring management
- RAII-based resource management throughout
- Error handling uses negative errno values following Linux conventions

## Build Requirements

- Linux with io_uring support (kernel 5.1+)
- liburing development libraries
- C++20 compatible compiler
- Standard networking headers (sys/socket.h, netdb.h)