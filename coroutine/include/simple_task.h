#pragma once

#include <coroutine>
#include <exception>
#include <utility>
#include <thread>

namespace co_uring {

struct simple_task {
    struct promise_type {
        simple_task get_return_object() {
            return simple_task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {
            exception_ = std::current_exception();
        }
        
        std::exception_ptr exception_;
    };
    
    using handle_type = std::coroutine_handle<promise_type>;
    
    simple_task() = default;
    explicit simple_task(handle_type h) : handle_(h) {}
    
    simple_task(const simple_task&) = delete;
    simple_task& operator=(const simple_task&) = delete;
    
    simple_task(simple_task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    simple_task& operator=(simple_task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }
    
    ~simple_task() {
        if (handle_) {
            handle_.destroy();
        }
    }
    
    bool done() const {
        return handle_ && handle_.done();
    }
    
    auto operator co_await() {
        struct awaiter {
            handle_type handle;
            
            bool await_ready() { return handle.done(); }
            void await_suspend(std::coroutine_handle<> caller) {}
            void await_resume() {
                if (handle.promise().exception_) {
                    std::rethrow_exception(handle.promise().exception_);
                }
            }
        };
        
        return awaiter{handle_};
    }
    
    handle_type handle_;
};

// Note: simple_task is a simpler version of task<void> without template parameters

template<typename T>
void spawn(T&& t) {
    // Fire-and-forget task execution
}

template<typename T>
void wait(T& t) {
    // Simple wait implementation
    while (t.handle_ && !t.handle_.done()) {
        std::this_thread::yield();
    }
}

} // namespace co_uring