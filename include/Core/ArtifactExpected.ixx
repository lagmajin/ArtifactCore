module;

#include <cstdlib>
#include <type_traits>
#include <utility>

export module Core.ArtifactExpected;

import Core.ArtifactOptional;
import Core.ArtifactString;

export namespace ArtifactCore {

enum class ArtifactExpectedErrorCode {
    Unknown,
    InvalidArgument,
    NotFound,
    Failed
};

struct ArtifactExpectedError {
    ArtifactExpectedErrorCode code = ArtifactExpectedErrorCode::Unknown;
    ZeroString message;
};

template<typename T, typename E = ArtifactExpectedError>
class ArtifactExpected {
public:
    using value_type = T;
    using error_type = E;

    ArtifactExpected(const T& value) : value_(value), hasValue_(true) {}
    ArtifactExpected(T&& value) : value_(std::move(value)), hasValue_(true) {}
    ArtifactExpected(const E& error) : error_(error), hasValue_(false) {}
    ArtifactExpected(E&& error) : error_(std::move(error)), hasValue_(false) {}

    [[nodiscard]] bool hasValue() const noexcept { return hasValue_; }
    [[nodiscard]] bool hasError() const noexcept { return !hasValue_; }
    explicit operator bool() const noexcept { return hasValue_; }

    T& value() & { ensureValue(); return *value_; }
    const T& value() const& { ensureValue(); return *value_; }
    T&& value() && { ensureValue(); return std::move(*value_); }
    const E& error() const& { ensureError(); return error_; }
    E& error() & { ensureError(); return error_; }

    template<typename U>
    T valueOr(U&& fallback) const& {
        return hasValue_ ? *value_ : static_cast<T>(std::forward<U>(fallback));
    }

    template<typename U>
    E errorOr(U&& fallback) const& {
        return hasValue_ ? static_cast<E>(std::forward<U>(fallback)) : error_;
    }

    ArtifactOptional<T> toOptional() const {
        return hasValue_ ? value_ : ArtifactOptional<T>{};
    }

    template<typename F>
    auto andThen(F&& fn) const -> decltype(fn(std::declval<const T&>())) {
        using R = decltype(fn(std::declval<const T&>()));
        return hasValue_ ? fn(*value_) : R(error_);
    }

    template<typename F>
    ArtifactExpected orElse(F&& fn) const {
        return hasValue_ ? *this : fn(error_);
    }

    template<typename F>
    auto transform(F&& fn) const
        -> ArtifactExpected<std::decay_t<decltype(fn(std::declval<const T&>()))>, E> {
        using U = std::decay_t<decltype(fn(std::declval<const T&>()))>;
        return hasValue_ ? ArtifactExpected<U, E>(fn(*value_))
                         : ArtifactExpected<U, E>(error_);
    }

    template<typename F>
    auto transformError(F&& fn) const
        -> ArtifactExpected<T, std::decay_t<decltype(fn(std::declval<const E&>()))>> {
        using U = std::decay_t<decltype(fn(std::declval<const E&>()))>;
        return hasValue_ ? ArtifactExpected<T, U>(*value_)
                         : ArtifactExpected<T, U>(fn(error_));
    }

private:
    void ensureValue() const { if (!hasValue_) std::abort(); }
    void ensureError() const { if (hasValue_) std::abort(); }

    ArtifactOptional<T> value_;
    E error_{};
    bool hasValue_ = false;
};

} // namespace ArtifactCore
