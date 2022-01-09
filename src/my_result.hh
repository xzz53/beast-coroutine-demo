#ifndef MY_RESULT_HH_
#define MY_RESULT_HH_

#include <optional>
#include <string>
#include <variant>

template <typename T>
struct Ok {
    Ok(T value_): value(value_) {}

    T value {};
};

template <typename T>
struct Err {
    Err(T value_): value(value_) {}

    T value {};
};

template <typename T, typename E>
struct Result {
    Result(Ok<T> ok): m_var {ok} {}
    Result(Err<E> err): m_var {err} {}

    // required for boost::asio::experimental::channel;
    Result() {};

    bool is_ok() const {
        return m_var.index() == 1;
    }

    bool is_err() const {
        return m_var.index() == 2;
    }

    bool is_undefined() const {
        return m_var.index() = 0;
    }

    std::optional<T> ok() const {
        const auto *p = std::get_if<1>(&m_var);
        return p ? p->value : std::optional<T> {};
    }

    std::optional<E> err() const {
        const auto *p = std::get_if<2>(&m_var);
        return p ? p->value : std::optional<E> {};
    }

private:
    std::variant<std::monostate, Ok<T>, Err<E>> m_var;
};
#endif
