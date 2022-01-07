#ifndef MY_RESULT_HH_
#define MY_RESULT_HH_

#include <optional>
#include <string>
#include <variant>

template <typename T>
struct Ok {
    T value;
};

struct Err {
    std::string message;
};

template <typename T>
using Result = std::variant<Ok<T>, Err>;

template <typename T>
bool
is_ok(Result<T> r) {
    return r.index() == 0;
}

template <typename T>
bool
is_err(Result<T> r) {
    return r.index() == 1;
}

template <typename T>
std::optional<T>
ok(Result<T> r) {
    const auto *p = std::get_if<0>(&r);
    return p ? p->value : std::optional<T> {};
}

template <typename T>
std::optional<std::string>
err(Result<T> r) {
    const auto *p = std::get_if<1>(&r);
    return p ? p->message : std::optional<std::string> {};
}

#endif
