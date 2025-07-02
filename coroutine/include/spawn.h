#pragma once

#include <coroutine>
#include <exception>
#include <iostream>
#include <utility>
#include "../../io/include/logger.h"

namespace co_uring
{

    // Fire-and-forget coroutine type for spawn
    class spawn_task
    {
    public:
        struct promise_type
        {
            spawn_task get_return_object()
            {
                auto handle = std::coroutine_handle<promise_type>::from_promise(*this);
                LOG_DEBUG("🔄 spawn_task created - handle: 0x{:016x}", reinterpret_cast<uintptr_t>(handle.address()));
                return spawn_task{handle};
            }

            // 즉시 시작
            std::suspend_never initial_suspend() noexcept
            {
                LOG_DEBUG("🚀 spawn_task initial_suspend - starting immediately");
                return {};
            }

            // 완료 시 즉시 자동 정리
            std::suspend_never final_suspend() noexcept
            {
                LOG_DEBUG("🏁 spawn_task final_suspend - auto cleanup");
                return {};
            }

            void return_void()
            {
                LOG_DEBUG("✅ spawn_task return_void - coroutine completed normally");
            }

            void unhandled_exception()
            {
                LOG_ERROR("💥 spawn_task unhandled_exception - fatal error occurred");
                // 예외 발생 시 프로그램 종료
                try
                {
                    std::rethrow_exception(std::current_exception());
                }
                catch (const std::exception &e)
                {
                    LOG_ERROR("❌ Fatal: Unhandled exception in spawn_task: {}", e.what());
                    std::terminate();
                }
                catch (...)
                {
                    LOG_ERROR("❌ Fatal: Unknown exception in spawn_task");
                    std::terminate();
                }
            }
        };

        using handle_type = std::coroutine_handle<promise_type>;

        spawn_task() = default;
        explicit spawn_task(handle_type h) : handle_(h)
        {
            if (h)
            {
                LOG_DEBUG("📦 spawn_task constructor - handle: 0x{:016x}", reinterpret_cast<uintptr_t>(h.address()));
            }
        }

        // 복사 불가
        spawn_task(const spawn_task &) = delete;
        spawn_task &operator=(const spawn_task &) = delete;

        // 이동 가능
        spawn_task(spawn_task &&other) noexcept : handle_(std::exchange(other.handle_, {}))
        {
            if (handle_)
            {
                LOG_DEBUG("📦 spawn_task move constructor - handle: 0x{:016x}", reinterpret_cast<uintptr_t>(handle_.address()));
            }
        }

        spawn_task &operator=(spawn_task &&other) noexcept
        {
            if (this != &other)
            {
                if (handle_)
                {
                    LOG_DEBUG("🗑️ spawn_task move assignment - destroying old handle: 0x{:016x}", reinterpret_cast<uintptr_t>(handle_.address()));
                }
                handle_ = std::exchange(other.handle_, {});
                if (handle_)
                {
                    LOG_DEBUG("📦 spawn_task move assignment - new handle: 0x{:016x}", reinterpret_cast<uintptr_t>(handle_.address()));
                }
            }
            return *this;
        }

        ~spawn_task()
        {
            if (handle_)
            {
                LOG_DEBUG("🗑️ spawn_task destructor - handle: 0x{:016x} (auto-cleanup, no explicit destroy)", reinterpret_cast<uintptr_t>(handle_.address()));
            }
            // 자동 정리 - 코루틴이 완료되면 자동으로 destroy됨
            // 여기서는 명시적으로 destroy하지 않음
        }

    private:
        handle_type handle_;
    };

    // Fire-and-forget spawn 함수
    template <typename Awaitable>
    void spawn(Awaitable &&awaitable)
    {
        LOG_DEBUG("🌱 spawn() called - creating fire-and-forget coroutine");

        [](Awaitable awaitable) -> spawn_task
        {
            LOG_DEBUG("🔄 spawn lambda coroutine started");
            try
            {
                co_await std::move(awaitable);
                LOG_DEBUG("✅ spawn lambda coroutine completed successfully");
            }
            catch (const std::exception &e)
            {
                LOG_ERROR("💥 spawn lambda coroutine exception: {}", e.what());
                throw;
            }
            catch (...)
            {
                LOG_ERROR("💥 spawn lambda coroutine unknown exception");
                throw;
            }
        }(std::forward<Awaitable>(awaitable));

        LOG_DEBUG("🌱 spawn() completed - coroutine launched");
    }

} // namespace co_uring