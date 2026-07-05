module;

#include <initializer_list>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

export module Core.ArtifactOptional;

export namespace ArtifactCore {

struct ArtifactNullOpt {
    explicit constexpr ArtifactNullOpt(int) {}
};

inline constexpr ArtifactNullOpt ArtifactNullopt{0};

inline constexpr ArtifactNullOpt Nullopt{0};

template<typename T>
class ArtifactOptional {
public:
    using value_type = T;
    
    ArtifactOptional() noexcept : hasValue_(false), dummy_{} {}
    
    explicit ArtifactOptional(ArtifactNullOpt) noexcept : hasValue_(false), dummy_{} {}
    
    ArtifactOptional(const T& value) : hasValue_(true), contained_(value) {}
    
    ArtifactOptional(T&& value) : hasValue_(true), contained_(std::move(value)) {}
    
    ArtifactOptional(const ArtifactOptional& other) : hasValue_(other.hasValue_) {
        if (hasValue_) {
            new (&contained_) T(other.contained_);
        }
    }
    
    ArtifactOptional(ArtifactOptional&& other) noexcept : hasValue_(other.hasValue_) {
        if (hasValue_) {
            new (&contained_) T(std::move(other.contained_));
        }
    }
    
    template<typename U>
    explicit ArtifactOptional(std::initializer_list<U> init) : hasValue_(true) {
        if (init.size() > 0) {
            new (&contained_) T(*init.begin());
        }
    }
    
    ArtifactOptional& operator=(const ArtifactOptional& other) {
        if (this != &other) {
            if (hasValue_ && !other.hasValue_) {
                destroy();
            } else if (hasValue_ && other.hasValue_) {
                contained_ = other.contained_;
            } else if (!hasValue_ && other.hasValue_) {
                new (&contained_) T(other.contained_);
                hasValue_ = true;
            }
        }
        return *this;
    }
    
    ArtifactOptional& operator=(ArtifactOptional&& other) noexcept {
        if (this != &other) {
            if (hasValue_ && !other.hasValue_) {
                destroy();
            } else if (hasValue_ && other.hasValue_) {
                contained_ = std::move(other.contained_);
            } else if (!hasValue_ && other.hasValue_) {
                new (&contained_) T(std::move(other.contained_));
                hasValue_ = true;
            }
        }
        return *this;
    }
    
    ArtifactOptional& operator=(const T& value) {
        if (hasValue_) {
            contained_ = value;
        } else {
            new (&contained_) T(value);
            hasValue_ = true;
        }
        return *this;
    }
    
    ArtifactOptional& operator=(T&& value) {
        if (hasValue_) {
            contained_ = std::move(value);
        } else {
            new (&contained_) T(std::move(value));
            hasValue_ = true;
        }
        return *this;
    }
    
    ArtifactOptional& operator=(ArtifactNullOpt) noexcept {
        reset();
        return *this;
    }
    
    ~ArtifactOptional() { reset(); }
    
    T* operator->() noexcept { return &contained_; }
    const T* operator->() const noexcept { return &contained_; }
    
    T& operator*() & noexcept { return contained_; }
    const T& operator*() const& noexcept { return contained_; }
    
    T&& operator*() && noexcept { return std::move(contained_); }
    const T&& operator*() const&& noexcept { return std::move(contained_); }
    
    T& value() & {
        if (!hasValue_) throw std::bad_optional_access();
        return contained_;
    }
    
    const T& value() const& {
        if (!hasValue_) throw std::bad_optional_access();
        return contained_;
    }
    
    T&& value() && {
        if (!hasValue_) throw std::bad_optional_access();
        return std::move(contained_);
    }
    
    const T&& value() const&& {
        if (!hasValue_) throw std::bad_optional_access();
        return std::move(contained_);
    }
    
    template<typename U>
    T value_or(U&& defaultValue) const& {
        return hasValue_ ? contained_ : static_cast<T>(std::forward<U>(defaultValue));
    }
    
    template<typename U>
    T value_or(U&& defaultValue) && {
        return hasValue_ ? std::move(contained_) : static_cast<T>(std::forward<U>(defaultValue));
    }
    
    void swap(ArtifactOptional& other) noexcept {
        if (hasValue_ && other.hasValue_) {
            using std::swap;
            swap(contained_, other.contained_);
        } else if (hasValue_ && !other.hasValue_) {
            other.contained_ = std::move(contained_);
            hasValue_ = false;
            other.hasValue_ = true;
        } else if (!hasValue_ && other.hasValue_) {
            contained_ = std::move(other.contained_);
            hasValue_ = true;
            other.hasValue_ = false;
        }
    }
    
    bool has_value() const noexcept { return hasValue_; }
    explicit operator bool() const noexcept { return hasValue_; }
    
    void reset() noexcept {
        if (hasValue_) {
            destroy();
            hasValue_ = false;
        }
    }
    
private:
    bool hasValue_;
    union {
        T contained_;
        char dummy_;
    };
    
    void destroy() noexcept {
        contained_.~T();
    }
};

template<typename T>
class ArtifactOptional<T&> {
public:
    using value_type = T;

    ArtifactOptional() noexcept = default;
    explicit ArtifactOptional(ArtifactNullOpt) noexcept {}
    ArtifactOptional(T& value) noexcept : ptr_(std::addressof(value)) {}
    ArtifactOptional(const ArtifactOptional&) noexcept = default;
    ArtifactOptional(ArtifactOptional&&) noexcept = default;

    ArtifactOptional& operator=(const ArtifactOptional&) noexcept = default;
    ArtifactOptional& operator=(ArtifactOptional&&) noexcept = default;

    ArtifactOptional& operator=(ArtifactNullOpt) noexcept {
        reset();
        return *this;
    }

    ArtifactOptional& operator=(T& value) noexcept {
        ptr_ = std::addressof(value);
        return *this;
    }

    T& operator*() const noexcept { return *ptr_; }
    T* operator->() const noexcept { return ptr_; }

    T& value() const {
        if (!ptr_) throw std::bad_optional_access();
        return *ptr_;
    }

    T& value_or(T& defaultValue) const noexcept {
        return ptr_ ? *ptr_ : defaultValue;
    }

    bool has_value() const noexcept { return ptr_ != nullptr; }
    explicit operator bool() const noexcept { return has_value(); }

    void reset() noexcept { ptr_ = nullptr; }

private:
    T* ptr_ = nullptr;
};

template<typename T>
class ArtifactOptional<const T&> {
public:
    using value_type = const T;

    ArtifactOptional() noexcept = default;
    explicit ArtifactOptional(ArtifactNullOpt) noexcept {}
    ArtifactOptional(const T& value) noexcept : ptr_(std::addressof(value)) {}
    ArtifactOptional(const ArtifactOptional&) noexcept = default;
    ArtifactOptional(ArtifactOptional&&) noexcept = default;

    ArtifactOptional& operator=(const ArtifactOptional&) noexcept = default;
    ArtifactOptional& operator=(ArtifactOptional&&) noexcept = default;

    ArtifactOptional& operator=(ArtifactNullOpt) noexcept {
        reset();
        return *this;
    }

    ArtifactOptional& operator=(const T& value) noexcept {
        ptr_ = std::addressof(value);
        return *this;
    }

    const T& operator*() const noexcept { return *ptr_; }
    const T* operator->() const noexcept { return ptr_; }

    const T& value() const {
        if (!ptr_) throw std::bad_optional_access();
        return *ptr_;
    }

    const T& value_or(const T& defaultValue) const noexcept {
        return ptr_ ? *ptr_ : defaultValue;
    }

    bool has_value() const noexcept { return ptr_ != nullptr; }
    explicit operator bool() const noexcept { return has_value(); }

    void reset() noexcept { ptr_ = nullptr; }

private:
    const T* ptr_ = nullptr;
};

template<typename T>
using Optional = ArtifactOptional<T>;

template<typename T, typename... Args>
inline ArtifactOptional<T> make_optional(Args&&... args) {
    return ArtifactOptional<T>(T(std::forward<Args>(args)...));
}

} // namespace ArtifactCore
