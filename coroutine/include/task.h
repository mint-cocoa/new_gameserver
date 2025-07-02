#pragma once

#include <coroutine>
#include <exception>
#include <utility>
#include <thread>
#include <iostream>
#include "../../io/include/logger.h"

namespace co_uring
{

    template <typename T>
    class task
    {
    public:
        struct promise_type
        {
            task get_return_object()
            {
                auto handle = std::coroutine_handle<promise_type>::from_promise(*this);
                LOG_DEBUG("🔄 task<T> created - handle: 0x{:016x}", reinterpret_cast<uintptr_t>(handle.address()));
                return task{handle};
            }

            std::suspend_never initial_suspend()
            {
                LOG_DEBUG("🚀 task<T> initial_suspend - starting immediately");
                return {};
            }

            std::suspend_never final_suspend() noexcept
            {
                LOG_DEBUG("🏁 task<T> final_suspend - auto cleanup");
                return {};
            }

            template <typename U>
            void return_value(U &&value)
            {
                LOG_DEBUG("📤 task<T> return_value - coroutine returning value");
                result_ = std::forward<U>(value);
            }

            void unhandled_exception()
            {
                LOG_ERROR("💥 task<T> unhandled_exception - storing exception");
                exception_ = std::current_exception();
            }

            [[no_unique_address]] T result_{};
            std::exception_ptr exception_;
        };

        using handle_type = std::coroutine_handle<promise_type>;

        task() = default;
        explicit task(handle_type h) : handle_(h)
        {
            if (h)
            {
                LOG_DEBUG("📦 task<T> constructor - handle: 0x{:016x}", reinterpret_cast<uintptr_t>(h.address()));
            }
        }

        task(const task &) = delete;
        task &operator=(const task &) = delete;

        task(task &&other) noexcept : handle_(std::exchange(other.handle_, {}))
        {
            if (handle_)
            {
                LOG_DEBUG("📦 task<T> move constructor - handle: 0x{:016x}", reinterpret_cast<uintptr_t>(handle_.address()));
            }
        }

        task &operator=(task &&other) noexcept
        {
            if (this != &other)
            {
                if (handle_)
                {
                    LOG_DEBUG("🗑️ task<T> move assignment - destroying old handle: 0x{:016x}", reinterpret_cast<uintptr_t>(handle_.address()));
                    handle_.destroy();
                }
                handle_ = std::exchange(other.handle_, {});
                if (handle_)
                {
                    LOG_DEBUG("📦 task<T> move assignment - new handle: 0x{:016x}", reinterpret_cast<uintptr_t>(handle_.address()));
                }
            }
            return *this;
        }

        ~task()
        {
            if (handle_)
            {
                LOG_DEBUG("🗑️ task<T> destructor - destroying handle: 0x{:016x}", reinterpret_cast<uintptr_t>(handle_.address()));
                handle_.destroy();
            }
        }

        bool done() const
        {
            bool is_done = handle_ && handle_.done();
            LOG_DEBUG("❓ task<T> done() check - handle: 0x{:016x}, done: {}",
                      handle_ ? reinterpret_cast<uintptr_t>(handle_.address()) : 0, is_done);
            return is_done;
        }

        T get()
        {
            if (!handle_)
            {
                LOG_ERROR("❌ task<T> get() - invalid handle");
                throw std::runtime_error("Invalid task handle");
            }

            if (handle_.promise().exception_)
            {
                LOG_ERROR("💥 task<T> get() - rethrowing stored exception");
                std::rethrow_exception(handle_.promise().exception_);
            }

            LOG_DEBUG("📤 task<T> get() - returning result");
            return std::move(handle_.promise().result_);
        }

        auto operator co_await()
        {
            struct awaiter
            {
                handle_type handle;

                bool await_ready()
                {
                    bool ready = handle.done();
                    LOG_DEBUG("❓ task<T> await_ready - handle: 0x{:016x}, ready: {}",
                              reinterpret_cast<uintptr_t>(handle.address()), ready);
                    return ready;
                }

                void await_suspend(std::coroutine_handle<> caller)
                {
                    LOG_DEBUG("⏸️ task<T> await_suspend - caller: 0x{:016x}, task: 0x{:016x}",
                              reinterpret_cast<uintptr_t>(caller.address()),
                              reinterpret_cast<uintptr_t>(handle.address()));
                    // Simple fire-and-forget for now
                }

                T await_resume()
                {
                    LOG_DEBUG("▶️ task<T> await_resume - handle: 0x{:016x}", reinterpret_cast<uintptr_t>(handle.address()));
                    if (handle.promise().exception_)
                    {
                        LOG_ERROR("💥 task<T> await_resume - rethrowing stored exception");
                        std::rethrow_exception(handle.promise().exception_);
                    }
                    LOG_DEBUG("📤 task<T> await_resume - returning result");
                    return std::move(handle.promise().result_);
                }
            };

            LOG_DEBUG("🔄 task<T> co_await - creating awaiter for handle: 0x{:016x}",
                      handle_ ? reinterpret_cast<uintptr_t>(handle_.address()) : 0);
            return awaiter{handle_};
        }

    private:
        handle_type handle_;
    };

    // Template specialization for void
    template <>
    class task<void>
    {
    public:
        struct promise_type
        {
            task get_return_object()
            {
                auto handle = std::coroutine_handle<promise_type>::from_promise(*this);
                LOG_DEBUG("🔄 task<void> created - handle: 0x{:016x}", reinterpret_cast<uintptr_t>(handle.address()));
                return task{handle};
            }

            std::suspend_never initial_suspend()
            {
                LOG_DEBUG("🚀 task<void> initial_suspend - starting immediately");
                return {};
            }

            std::suspend_never final_suspend() noexcept
            {
                LOG_DEBUG("🏁 task<void> final_suspend - auto cleanup");
                return {};
            }

            void return_void()
            {
                LOG_DEBUG("📤 task<void> return_void - coroutine completed");
            }

            void unhandled_exception()
            {
                LOG_ERROR("💥 task<void> unhandled_exception - storing exception");
                exception_ = std::current_exception();
            }

            std::exception_ptr exception_;
        };

        using handle_type = std::coroutine_handle<promise_type>;

        task() = default;
        explicit task(handle_type h) : handle_(h)
        {
            if (h)
            {
                LOG_DEBUG("📦 task<void> constructor - handle: 0x{:016x}", reinterpret_cast<uintptr_t>(h.address()));
            }
        }

        task(const task &) = delete;
        task &operator=(const task &) = delete;

        task(task &&other) noexcept : handle_(std::exchange(other.handle_, {}))
        {
            if (handle_)
            {
                LOG_DEBUG("📦 task<void> move constructor - handle: 0x{:016x}", reinterpret_cast<uintptr_t>(handle_.address()));
            }
        }

        task &operator=(task &&other) noexcept
        {
            if (this != &other)
            {
                if (handle_)
                {
                    LOG_DEBUG("🗑️ task<void> move assignment - destroying old handle: 0x{:016x}", reinterpret_cast<uintptr_t>(handle_.address()));
                    handle_.destroy();
                }
                handle_ = std::exchange(other.handle_, {});
                if (handle_)
                {
                    LOG_DEBUG("📦 task<void> move assignment - new handle: 0x{:016x}", reinterpret_cast<uintptr_t>(handle_.address()));
                }
            }
            return *this;
        }

        ~task()
        {
            if (handle_)
            {
                LOG_DEBUG("🗑️ task<void> destructor - destroying handle: 0x{:016x}", reinterpret_cast<uintptr_t>(handle_.address()));
                handle_.destroy();
            }
        }

        bool done() const
        {
            bool is_done = handle_ && handle_.done();
            LOG_DEBUG("❓ task<void> done() check - handle: 0x{:016x}, done: {}",
                      handle_ ? reinterpret_cast<uintptr_t>(handle_.address()) : 0, is_done);
            return is_done;
        }

        void get()
        {
            if (!handle_)
            {
                LOG_ERROR("❌ task<void> get() - invalid handle");
                throw std::runtime_error("Invalid task handle");
            }

            if (handle_.promise().exception_)
            {
                LOG_ERROR("💥 task<void> get() - rethrowing stored exception");
                std::rethrow_exception(handle_.promise().exception_);
            }

            LOG_DEBUG("✅ task<void> get() - completed successfully");
        }

        auto operator co_await()
        {
            struct awaiter
            {
                handle_type handle;

                bool await_ready()
                {
                    bool ready = handle.done();
                    LOG_DEBUG("❓ task<void> await_ready - handle: 0x{:016x}, ready: {}",
                              reinterpret_cast<uintptr_t>(handle.address()), ready);
                    return ready;
                }

                void await_suspend(std::coroutine_handle<> caller)
                {
                    LOG_DEBUG("⏸️ task<void> await_suspend - caller: 0x{:016x}, task: 0x{:016x}",
                              reinterpret_cast<uintptr_t>(caller.address()),
                              reinterpret_cast<uintptr_t>(handle.address()));
                    // Simple fire-and-forget for now
                }

                void await_resume()
                {
                    LOG_DEBUG("▶️ task<void> await_resume - handle: 0x{:016x}", reinterpret_cast<uintptr_t>(handle.address()));
                    if (handle.promise().exception_)
                    {
                        LOG_ERROR("💥 task<void> await_resume - rethrowing stored exception");
                        std::rethrow_exception(handle.promise().exception_);
                    }
                    LOG_DEBUG("✅ task<void> await_resume - completed successfully");
                }
            };

            LOG_DEBUG("🔄 task<void> co_await - creating awaiter for handle: 0x{:016x}",
                      handle_ ? reinterpret_cast<uintptr_t>(handle_.address()) : 0);
            return awaiter{handle_};
        }

    private:
        handle_type handle_;
    };

    // Helper functions
    template <typename T>
    void wait(task<T> &t)
    {
        // Simple wait implementation
        while (t.handle_ && !t.handle_.done())
        {
            // Busy wait for now - in real implementation this would yield
            std::this_thread::yield();
        }
    }

    // Type alias for backward compatibility
    using simple_task = task<void>;

} // namespace co_uring