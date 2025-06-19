#pragma once

#include <coroutine>
#include <exception>
#include <utility>
#include <thread>

namespace co_uring {

template<typename T>
class task {
public:
    struct promise_type {
        task get_return_object() {
            return task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        
        template<typename U>
        void return_value(U&& value) {
            result_ = std::forward<U>(value);
        }
        
        void unhandled_exception() {
            exception_ = std::current_exception();
        }
        
        [[no_unique_address]] T result_{};
        std::exception_ptr exception_;
    };
    
    using handle_type = std::coroutine_handle<promise_type>;
    
    task() = default;
    explicit task(handle_type h) : handle_(h) {}
    
    task(const task&) = delete;
    task& operator=(const task&) = delete;
    
    task(task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    task& operator=(task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }
    
    ~task() {
        if (handle_) {
            handle_.destroy();
        }
    }
    
    bool done() const {
        return handle_ && handle_.done();
    }
    
    T get() {
        if (!handle_) {
            throw std::runtime_error("Invalid task handle");
        }
        
        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
        
        return std::move(handle_.promise().result_);
    }
    
    auto operator co_await() {
        struct awaiter {
            handle_type handle;
            
            bool await_ready() { return handle.done(); }
            
            void await_suspend(std::coroutine_handle<> caller) {
                // Simple fire-and-forget for now
            }
            
            T await_resume() {
                if (handle.promise().exception_) {
                    std::rethrow_exception(handle.promise().exception_);
                }
                return std::move(handle.promise().result_);
            }
        };
        
        return awaiter{handle_};
    }
    
private:
    handle_type handle_;
};

// Template specialization for void
template<>
class task<void> {
public:
    struct promise_type {
        task get_return_object() {
            return task{std::coroutine_handle<promise_type>::from_promise(*this)};
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
    
    task() = default;
    explicit task(handle_type h) : handle_(h) {}
    
    task(const task&) = delete;
    task& operator=(const task&) = delete;
    
    task(task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    task& operator=(task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }
    
    ~task() {
        if (handle_) {
            handle_.destroy();
        }
    }
    
    bool done() const {
        return handle_ && handle_.done();
    }
    
    void get() {
        if (!handle_) {
            throw std::runtime_error("Invalid task handle");
        }
        
        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
    }
    
    auto operator co_await() {
        struct awaiter {
            handle_type handle;
            
            bool await_ready() { return handle.done(); }
            
            void await_suspend(std::coroutine_handle<> caller) {
                // Simple fire-and-forget for now
            }
            
            void await_resume() {
                if (handle.promise().exception_) {
                    std::rethrow_exception(handle.promise().exception_);
                }
            }
        };
        
        return awaiter{handle_};
    }
    
private:
    handle_type handle_;
};

// Helper functions
template<typename T>
void spawn(task<T>&& t) {
    // Fire-and-forget task execution - just let the task destructor handle cleanup
    (void)t; // Mark as used to avoid warnings
}

template<typename T>
void wait(task<T>& t) {
    // Simple wait implementation
    while (t.handle_ && !t.handle_.done()) {
        // Busy wait for now - in real implementation this would yield
        std::this_thread::yield();
    }
}

} // namespace co_uring