#include <coroutine>
#include <cstddef>
#include <functional>
#include <iterator>
#include <optional>

namespace fhamonic {
namespace melon {

template <typename _Value>
class generator {
    class promise {
    public:
        using value_type = _Value;

        promise() = default;
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void unhandled_exception() noexcept {}
        void return_void() noexcept {}

        std::suspend_always yield_value(_Value value) noexcept {
            this->value = std::move(value);
            return {};
        }
        const value_type get_value() noexcept { return value; }

        inline generator get_return_object();

    private:
        value_type value{};
    };

public:
    using value_type = _Value;
    using promise_type = promise;

    explicit generator(std::coroutine_handle<promise> handle)
        : handle(handle) {}

    struct end_iterator {};

    class iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = _Value;

        iterator() noexcept = default;
        iterator(std::coroutine_handle<promise> & h) noexcept : handle{h} {}

        value_type operator*() const noexcept {
            return handle.promise().get_value();
        }

        iterator & operator++() {
            handle.resume();
            return *this;
        }
        void operator++(int) { (void)operator++(); }

        friend bool operator==(const iterator & it, end_iterator) noexcept {
            return it.handle.done();
        }

    private:
        std::coroutine_handle<promise> & handle;
    };

    iterator begin() {
        handle.resume();
        return iterator{handle};
    }
    end_iterator end() noexcept { return {}; }

private:
    std::coroutine_handle<promise> handle;
};

template <typename _Value>
inline generator<_Value> generator<_Value>::promise::get_return_object() {
    return generator{std::coroutine_handle<promise>::from_promise(*this)};
}

}  // namespace melon
}  // namespace fhamonic
