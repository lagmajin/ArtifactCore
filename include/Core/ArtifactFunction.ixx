module;
#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

export module Core.ArtifactFunction;

export namespace ArtifactCore {

namespace detail {

template<typename R, typename... Args>
using Invoker = R(*)(void*, Args...);

template<typename F, typename R, typename... Args>
R invokeStub(void* ptr, Args... args) {
    return (*static_cast<F*>(ptr))(std::forward<Args>(args)...);
}

template<typename F>
void destroyStub(void* ptr) {
    static_cast<F*>(ptr)->~F();
}

template<typename F>
void moveStub(void* src, void* dst) {
    ::new (dst) F(std::move(*static_cast<F*>(src)));
}

} // namespace detail

template<typename Signature>
class ArtifactFunction;

template<typename R, typename... Args>
class ArtifactFunction<R(Args...)> {
private:
    static constexpr size_t SBO_SIZE = 32;

    using InvokerFn = R(*)(void*, Args...);
    using DestroyerFn = void(*)(void*);
    using MoverFn = void(*)(void*, void*);

    alignas(std::max_align_t) unsigned char storage_[SBO_SIZE];
    InvokerFn invoker_ = nullptr;
    DestroyerFn destroyer_ = nullptr;
    MoverFn mover_ = nullptr;
    bool onHeap_ = false;

    template<typename F>
    static constexpr bool fitsSbo() {
        return sizeof(F) <= SBO_SIZE && alignof(F) <= alignof(std::max_align_t);
    }

    void* ptr() { return onHeap_ ? *reinterpret_cast<void**>(storage_) : static_cast<void*>(storage_); }
    const void* ptr() const { return onHeap_ ? *reinterpret_cast<void* const*>(storage_) : static_cast<const void*>(storage_); }

    void clear() noexcept {
        if (invoker_) {
            if (destroyer_) destroyer_(ptr());
            if (onHeap_) {
                ::operator delete(*reinterpret_cast<void**>(storage_));
            }
        }
        invoker_ = nullptr;
        destroyer_ = nullptr;
        mover_ = nullptr;
        onHeap_ = false;
    }

public:
    ArtifactFunction() noexcept = default;

    template<typename F, typename = std::enable_if_t<
        !std::is_same_v<std::decay_t<F>, ArtifactFunction> &&
        std::is_invocable_r_v<R, F, Args...>>>
    ArtifactFunction(F&& f) {
        using DecayedF = std::decay_t<F>;
        if constexpr (fitsSbo<DecayedF>()) {
            ::new (storage_) DecayedF(std::forward<F>(f));
            onHeap_ = false;
        } else {
            auto* heap = new DecayedF(std::forward<F>(f));
            *reinterpret_cast<DecayedF**>(storage_) = heap;
            onHeap_ = true;
        }
        invoker_ = &detail::invokeStub<DecayedF, R, Args...>;
        destroyer_ = &detail::destroyStub<DecayedF>;
        mover_ = &detail::moveStub<DecayedF>;
    }

    ArtifactFunction(ArtifactFunction&& other) noexcept {
        if (other.invoker_) {
            invoker_ = other.invoker_;
            destroyer_ = other.destroyer_;
            mover_ = other.mover_;
            onHeap_ = other.onHeap_;
            if (other.mover_) {
                other.mover_(other.ptr(), ptr());
            }
            other.invoker_ = nullptr;
            other.destroyer_ = nullptr;
            other.mover_ = nullptr;
            other.onHeap_ = false;
        }
    }

    ArtifactFunction& operator=(ArtifactFunction&& other) noexcept {
        if (this != &other) {
            clear();
            if (other.invoker_) {
                invoker_ = other.invoker_;
                destroyer_ = other.destroyer_;
                mover_ = other.mover_;
                onHeap_ = other.onHeap_;
                if (other.mover_) {
                    other.mover_(other.ptr(), ptr());
                }
                other.invoker_ = nullptr;
                other.destroyer_ = nullptr;
                other.mover_ = nullptr;
                other.onHeap_ = false;
            }
        }
        return *this;
    }

    ArtifactFunction(const ArtifactFunction&) = delete;
    ArtifactFunction& operator=(const ArtifactFunction&) = delete;

    ~ArtifactFunction() { clear(); }

    explicit operator bool() const noexcept { return invoker_ != nullptr; }

    R operator()(Args... args) {
        return invoker_(ptr(), std::forward<Args>(args)...);
    }

    void swap(ArtifactFunction& other) noexcept {
        ArtifactFunction tmp(std::move(other));
        other = std::move(*this);
        *this = std::move(tmp);
    }
};

} // namespace ArtifactCore
