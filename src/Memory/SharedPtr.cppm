module;

#include <memory>
#include <utility>

export module Memory.SharedPtr;

namespace ArtifactCore {

namespace Memory {

template<typename T>
class WeakPtr;

template<typename T>
class SharedPtr {
public:
    using element_type = T;

    SharedPtr() noexcept = default;
    SharedPtr(std::nullptr_t) noexcept {}
    SharedPtr(const std::shared_ptr<T>& ptr) noexcept : ptr_(ptr) {}
    SharedPtr(std::shared_ptr<T>&& ptr) noexcept : ptr_(std::move(ptr)) {}

    template<typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    SharedPtr(const std::shared_ptr<U>& ptr) noexcept : ptr_(ptr) {}

    T* get() const noexcept { return ptr_.get(); }
    T* operator->() const noexcept { return ptr_.get(); }
    T& operator*() const noexcept { return *ptr_; }
    explicit operator bool() const noexcept { return static_cast<bool>(ptr_); }

    std::shared_ptr<T> lock() const noexcept { return ptr_; }
    long useCount() const noexcept { return ptr_.use_count(); }

    void reset() noexcept { ptr_.reset(); }
    void swap(SharedPtr& other) noexcept { ptr_.swap(other.ptr_); }

    SharedPtr(const SharedPtr&) = default;
    SharedPtr(SharedPtr&&) noexcept = default;
    SharedPtr& operator=(const SharedPtr&) = default;
    SharedPtr& operator=(SharedPtr&&) noexcept = default;

    SharedPtr& operator=(std::nullptr_t) noexcept {
        reset();
        return *this;
    }

    SharedPtr& operator=(const std::shared_ptr<T>& ptr) noexcept {
        ptr_ = ptr;
        return *this;
    }

    friend bool operator==(const SharedPtr& a, const SharedPtr& b) noexcept {
        return a.ptr_ == b.ptr_;
    }

    friend bool operator!=(const SharedPtr& a, const SharedPtr& b) noexcept {
        return !(a == b);
    }

    friend bool operator==(const SharedPtr& a, std::nullptr_t) noexcept {
        return !a.ptr_;
    }

    friend bool operator==(std::nullptr_t, const SharedPtr& a) noexcept {
        return !a.ptr_;
    }

    friend bool operator!=(const SharedPtr& a, std::nullptr_t) noexcept {
        return static_cast<bool>(a.ptr_);
    }

    friend bool operator!=(std::nullptr_t, const SharedPtr& a) noexcept {
        return static_cast<bool>(a.ptr_);
    }

    operator std::shared_ptr<T>() const { return ptr_; }

private:
    std::shared_ptr<T> ptr_;
};

template<typename T>
class WeakPtr {
public:
    WeakPtr() noexcept = default;
    WeakPtr(std::nullptr_t) noexcept {}
    WeakPtr(const std::shared_ptr<T>& ptr) noexcept : ptr_(ptr) {}
    WeakPtr(const SharedPtr<T>& ptr) noexcept : ptr_(ptr.lock()) {}
    WeakPtr(const std::weak_ptr<T>& ptr) noexcept : ptr_(ptr) {}

    WeakPtr(const WeakPtr&) = default;
    WeakPtr(WeakPtr&&) noexcept = default;
    WeakPtr& operator=(const WeakPtr&) = default;
    WeakPtr& operator=(WeakPtr&&) noexcept = default;

    WeakPtr& operator=(const SharedPtr<T>& ptr) noexcept {
        ptr_ = ptr.lock();
        return *this;
    }

    WeakPtr& operator=(const std::shared_ptr<T>& ptr) noexcept {
        ptr_ = ptr;
        return *this;
    }

    WeakPtr& operator=(std::nullptr_t) noexcept {
        ptr_.reset();
        return *this;
    }

    bool expired() const noexcept { return ptr_.expired(); }
    long useCount() const noexcept { return ptr_.use_count(); }
    SharedPtr<T> lock() const noexcept { return SharedPtr<T>(ptr_.lock()); }
    void reset() noexcept { ptr_.reset(); }

private:
    std::weak_ptr<T> ptr_;
};

template<typename T, typename... Args>
inline SharedPtr<T> makeShared(Args&&... args) {
    return SharedPtr<T>(std::make_shared<T>(std::forward<Args>(args)...));
}

template<typename T>
inline SharedPtr<T> makeShared(const std::shared_ptr<T>& ptr) {
    return SharedPtr<T>(ptr);
}

template<typename T>
inline SharedPtr<T> makeShared(std::shared_ptr<T>&& ptr) {
    return SharedPtr<T>(std::move(ptr));
}

} // namespace Memory

export template<typename T>
using SharedPtr = Memory::SharedPtr<T>;

export template<typename T>
using WeakPtr = Memory::WeakPtr<T>;

export template<typename T, typename... Args>
inline SharedPtr<T> makeShared(Args&&... args) {
    return Memory::makeShared<T>(std::forward<Args>(args)...);
}

} // namespace ArtifactCore
