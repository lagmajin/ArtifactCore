module;

#include <cstddef>
#include <utility>
#include <type_traits>

export module Memory.UniquePtr;

namespace ArtifactCore {

namespace Memory {

template<typename T>
inline void defaultDelete(T* ptr) {
    delete ptr;
}

template<typename T>
class UniquePtr {
public:
    using element_type = T;
    using deleter_type = void(*)(T*);

    constexpr UniquePtr() noexcept
        : ptr_(nullptr), deleter_(&defaultDelete<T>) {}

    constexpr UniquePtr(std::nullptr_t) noexcept
        : ptr_(nullptr), deleter_(&defaultDelete<T>) {}

    explicit UniquePtr(T* ptr) noexcept
        : ptr_(ptr), deleter_(&defaultDelete<T>) {}

    UniquePtr(T* ptr, deleter_type deleter) noexcept
        : ptr_(ptr), deleter_(deleter ? deleter : &defaultDelete<T>) {}

    UniquePtr(const UniquePtr&) = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;

    UniquePtr(UniquePtr&& other) noexcept
        : ptr_(other.ptr_), deleter_(other.deleter_) {
        other.ptr_ = nullptr;
    }

    UniquePtr& operator=(UniquePtr&& other) noexcept {
        if (this != &other) {
            reset();
            ptr_ = other.ptr_;
            deleter_ = other.deleter_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    ~UniquePtr() {
        reset();
    }

    T* get() const noexcept { return ptr_; }
    T* operator->() const noexcept { return ptr_; }
    T& operator*() const noexcept { return *ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    T* release() noexcept {
        T* raw = ptr_;
        ptr_ = nullptr;
        return raw;
    }

    void reset(T* ptr = nullptr) noexcept {
        if (ptr_ != nullptr) {
            deleter_(ptr_);
        }
        ptr_ = ptr;
    }

    void swap(UniquePtr& other) noexcept {
        using std::swap;
        swap(ptr_, other.ptr_);
        swap(deleter_, other.deleter_);
    }

private:
    T* ptr_;
    deleter_type deleter_;
};

template<typename T>
inline UniquePtr<T> makeUnique(T* ptr) {
    return UniquePtr<T>(ptr);
}

template<typename T>
inline UniquePtr<T> makeUnique(T* ptr, typename UniquePtr<T>::deleter_type deleter) {
    return UniquePtr<T>(ptr, deleter);
}

template<typename T, typename... Args>
inline UniquePtr<T> makeUnique(Args&&... args) {
    return UniquePtr<T>(new T(std::forward<Args>(args)...));
}

} // namespace Memory

template<typename T>
using UniquePtr = Memory::UniquePtr<T>;

} // namespace ArtifactCore
