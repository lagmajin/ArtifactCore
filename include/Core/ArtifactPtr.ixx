module;
#include <cstdint>
#include <cassert>
#include <utility>

export module Core.ArtifactPtr;

import Core.ArtifactOptional;

export namespace ArtifactCore {

// --- RefCounted base for Ptr/Ref ---
namespace detail {
struct RefCount {
    int strong = 0;
    int weak = 0;
};
template <typename T>
struct RefBlock {
    RefCount count;
    T object;
    template <typename... Args>
    RefBlock(Args&&... args) : count{1, 0}, object(std::forward<Args>(args)...) {}
};
} // namespace detail

/// Nullable shared pointer. operator-> / operator* deleted — forces null check via get().
template <typename T>
class Ptr {
public:
    Ptr() = default;
    explicit Ptr(T* raw) : ptr_(raw), refs_(raw ? new detail::RefCount{1, 0} : nullptr) {}
    Ptr(const Ptr& other) : ptr_(other.ptr_), refs_(other.refs_) { if (refs_) ++refs_->strong; }
    Ptr(Ptr&& other) noexcept : ptr_(other.ptr_), refs_(other.refs_) { other.ptr_ = nullptr; other.refs_ = nullptr; }
    ~Ptr() { release(); }

    Ptr& operator=(const Ptr& other) {
        if (this == &other) return *this;
        release();
        ptr_ = other.ptr_; refs_ = other.refs_;
        if (refs_) ++refs_->strong;
        return *this;
    }
    Ptr& operator=(Ptr&& other) noexcept {
        if (this == &other) return *this;
        release();
        ptr_ = other.ptr_; refs_ = other.refs_;
        other.ptr_ = nullptr; other.refs_ = nullptr;
        return *this;
    }

    /// Returns reference if non-null. Forces null check.
    Optional<T&> get() { return ptr_ ? Optional<T&>(*ptr_) : Nullopt; }
    Optional<const T&> get() const { return ptr_ ? Optional<const T&>(*ptr_) : Nullopt; }

    /// Execute fn only if non-null.
    template <typename F> void ifValid(F&& fn) { if (ptr_) fn(*ptr_); }
    template <typename F> void ifValid(F&& fn) const { if (ptr_) fn(*ptr_); }

    bool isValid() const { return ptr_ != nullptr; }
    int useCount() const { return refs_ ? refs_->strong : 0; }
    void reset() { release(); ptr_ = nullptr; refs_ = nullptr; }

    // operator-> and operator* DELETED to force null checking
    T* operator->() = delete;
    T& operator*() = delete;

    template <typename U> friend class Ptr;
    template <typename U> friend class Ref;
    template <typename U> friend class WeakPtr;

private:
    T* ptr_ = nullptr;
    detail::RefCount* refs_ = nullptr;

    void release() {
        if (!refs_) return;
        --refs_->strong;
        if (refs_->strong == 0) {
            if (ptr_) delete ptr_;
            ptr_ = nullptr;
            if (refs_->weak == 0) delete refs_;
            else refs_->strong = -1;  // mark destroyed
        }
    }
};

/// Non-nullable shared reference. Cannot be default-constructed.
template <typename T>
class Ref {
public:
    explicit Ref(T* raw) : ptr_(raw), refs_(new detail::RefCount{1, 0}) { assert(raw && "Ref cannot be null"); }
    explicit Ref(const Ptr<T>& p) : ptr_(p.ptr_), refs_(p.refs_) { assert(ptr_ && "Ref cannot be null"); if (refs_) ++refs_->strong; }
    Ref(const Ref& other) : ptr_(other.ptr_), refs_(other.refs_) { if (refs_) ++refs_->strong; }
    Ref(Ref&& other) noexcept : ptr_(other.ptr_), refs_(other.refs_) { other.ptr_ = nullptr; other.refs_ = nullptr; }
    ~Ref() { release(); }

    Ref& operator=(const Ref& o) { if (this == &o) return *this; release(); ptr_=o.ptr_; refs_=o.refs_; if(refs_)++refs_->strong; return *this; }
    Ref& operator=(Ref&& o) noexcept { if(this==&o)return*this; release(); ptr_=o.ptr_; refs_=o.refs_; o.ptr_=nullptr; o.refs_=nullptr; return*this; }

    T* operator->() { return ptr_; }
    T& operator*() { return *ptr_; }
    const T* operator->() const { return ptr_; }
    const T& operator*() const { return *ptr_; }

private:
    T* ptr_ = nullptr;
    detail::RefCount* refs_ = nullptr;
    void release() {
        if (!refs_) return;
        --refs_->strong;
        if (refs_->strong == 0) { if (ptr_) delete ptr_; ptr_ = nullptr; if (refs_->weak == 0) delete refs_; else refs_->strong = -1; }
    }
};

/// Unique ownership. Non-copyable, movable.
template <typename T>
class Owned {
public:
    Owned() = default;
    explicit Owned(T* raw) : ptr_(raw) {}
    Owned(const Owned&) = delete;
    Owned& operator=(const Owned&) = delete;
    Owned(Owned&& o) noexcept : ptr_(o.ptr_) { o.ptr_ = nullptr; }
    Owned& operator=(Owned&& o) noexcept { if(this!=&o){delete ptr_; ptr_=o.ptr_; o.ptr_=nullptr;} return*this; }
    ~Owned() { delete ptr_; }

    Optional<T&> get() { return ptr_ ? Optional<T&>(*ptr_) : Nullopt; }
    Optional<const T&> get() const { return ptr_ ? Optional<const T&>(*ptr_) : Nullopt; }
    template <typename F> void ifValid(F&& fn) { if (ptr_) fn(*ptr_); }
    bool isValid() const { return ptr_ != nullptr; }
    T* take() { T* p = ptr_; ptr_ = nullptr; return p; }
    void reset(T* raw = nullptr) { delete ptr_; ptr_ = raw; }

    T* operator->() = delete;
    T& operator*() = delete;

private:
    T* ptr_ = nullptr;
};

/// Weak pointer — tracks Ref without preventing deletion.
template <typename T>
class WeakPtr {
public:
    WeakPtr() = default;
    explicit WeakPtr(const Ptr<T>& p) : ptr_(p.ptr_), refs_(p.refs_) { if (refs_) ++refs_->weak; }
    WeakPtr(const WeakPtr& o) : ptr_(o.ptr_), refs_(o.refs_) { if (refs_) ++refs_->weak; }
    ~WeakPtr() { release(); }

    Ptr<T> lock() const {
        if (!refs_ || refs_->strong <= 0) return Ptr<T>();
        Ptr<T> p;
        p.ptr_ = ptr_;
        p.refs_ = refs_;
        ++refs_->strong;
        return p;
    }
    bool isExpired() const { return !refs_ || refs_->strong <= 0; }

private:
    T* ptr_ = nullptr;
    detail::RefCount* refs_ = nullptr;
    void release() { if (refs_) { --refs_->weak; if (refs_->weak == 0 && refs_->strong <= 0) delete refs_; } }
};

} // namespace ArtifactCore
