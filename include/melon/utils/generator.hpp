#include <coroutine>
#include <cstddef>
#include <functional>
#include <iterator>
#include <optional>

template <typename T>
struct generator {
    using value_type = T;
    struct promise_type {
        using value_type = T;
        value_type current_value;
        std::suspend_always yield_value(value_type value) noexcept {
            this->current_value = value;
            return {};
        }
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        generator get_return_object() noexcept { return generator{this}; };
        void unhandled_exception() noexcept { std::terminate(); }
        void return_void() noexcept {}
    };

    struct end_iterator {};

    class iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using reference = T const &;
        using pointer = T *;

        iterator(std::coroutine_handle<promise_type> & h) : handle(h) {}
        iterator & operator++() noexcept {
            handle.resume();
            return *this;
        }
        friend bool operator==(const iterator & it, end_iterator) noexcept {
            return it.handle.done();
        }
        reference operator*() const noexcept {
            return handle.promise().current_value;
        }

    public:
        std::coroutine_handle<promise_type> & handle;
    };

    iterator begin() {
        handle.resume();
        return {handle};
    }
    end_iterator end() noexcept { return {}; }

    generator(generator const &) = delete;
    generator(generator && rhs) : handle(std::exchange(rhs.handle, nullptr)) {}

    ~generator() {
        if(handle) handle.destroy();
    }

private:
    explicit generator(promise_type * p)
        : handle(std::coroutine_handle<promise_type>::from_promise(*p)) {}

    std::coroutine_handle<promise_type> handle;
};