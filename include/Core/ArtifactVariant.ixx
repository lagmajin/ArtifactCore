module;
#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

export module Core.ArtifactVariant;

export namespace ArtifactCore {

namespace detail {

template<typename T, typename... Rest>
struct MaxTypeImpl;

template<typename T>
struct MaxTypeImpl<T> {
    static constexpr size_t size = sizeof(T);
    static constexpr size_t align = alignof(T);
};

template<typename T, typename U, typename... Rest>
struct MaxTypeImpl<T, U, Rest...> {
    static constexpr size_t size1 = sizeof(T) >= sizeof(U) ? sizeof(T) : sizeof(U);
    static constexpr size_t size = size1 >= MaxTypeImpl<Rest...>::size ? size1 : MaxTypeImpl<Rest...>::size;
    static constexpr size_t align1 = alignof(T) >= alignof(U) ? alignof(T) : alignof(U);
    static constexpr size_t align = align1 >= MaxTypeImpl<Rest...>::align ? align1 : MaxTypeImpl<Rest...>::align;
};

template<size_t I, typename T, typename... Rest>
struct TypeIndexImpl;

template<size_t I, typename T, typename... Rest>
struct TypeIndexImpl<I, T, T, Rest...> {
    static constexpr size_t value = I;
};

template<size_t I, typename T, typename U, typename... Rest>
struct TypeIndexImpl<I, T, U, Rest...> {
    static constexpr size_t value = TypeIndexImpl<I + 1, T, Rest...>::value;
};

template<typename T, typename... Rest>
struct Contains;

template<typename T, typename... Rest>
struct Contains<T, T, Rest...> {
    static constexpr bool value = true;
};

template<typename T, typename U, typename... Rest>
struct Contains<T, U, Rest...> {
    static constexpr bool value = Contains<T, Rest...>::value;
};

using DestroyFn = void(*)(void*);
using MoveFn = void(*)(void* src, void* dst);
using CopyFn = void(*)(const void* src, void* dst);

template<typename T>
void destroyImpl(void* ptr) {
    static_cast<T*>(ptr)->~T();
}

template<typename T>
void moveImpl(void* src, void* dst) {
    ::new (dst) T(std::move(*static_cast<T*>(src)));
}

template<typename T>
void copyImpl(const void* src, void* dst) {
    ::new (dst) T(*static_cast<const T*>(src));
}

struct Vtable {
    DestroyFn destroyer = nullptr;
    MoveFn mover = nullptr;
    CopyFn copier = nullptr;
};

template<typename T>
constexpr Vtable makeVtable() {
    return Vtable{
        &destroyImpl<T>,
        &moveImpl<T>,
        &copyImpl<T>
    };
}

template<typename... Types>
constexpr Vtable vtableFor(size_t index) {
    Vtable vtables[] = { makeVtable<Types>()... };
    return vtables[index];
}

} // namespace detail

template<typename... Types>
class ArtifactVariant {
private:
    static constexpr size_t StorageSize = detail::MaxTypeImpl<Types...>::size;
    static constexpr size_t StorageAlign = detail::MaxTypeImpl<Types...>::align;

    alignas(StorageAlign) unsigned char storage_[StorageSize];
    size_t typeIndex_ = 0;

    void* ptr() { return static_cast<void*>(storage_); }
    const void* ptr() const { return static_cast<const void*>(storage_); }

    detail::Vtable vtableForCurrent() const {
        return detail::vtableFor<Types...>(typeIndex_);
    }

    void destroy() {
        auto v = vtableForCurrent();
        if (v.destroyer) v.destroyer(ptr());
        typeIndex_ = 0;
    }

    void moveFrom(ArtifactVariant&& other) {
        typeIndex_ = other.typeIndex_;
        auto v = vtableForCurrent();
        if (v.mover) v.mover(other.ptr(), ptr());
    }

    void copyFrom(const ArtifactVariant& other) {
        typeIndex_ = other.typeIndex_;
        auto v = vtableForCurrent();
        if (v.copier) v.copier(other.ptr(), ptr());
    }

public:
    ArtifactVariant() : typeIndex_(0) {}

    template<typename T, typename = std::enable_if_t<detail::Contains<T, Types...>::value>>
    explicit ArtifactVariant(T&& value) {
        using DecayedT = std::decay_t<T>;
        typeIndex_ = detail::TypeIndexImpl<0, DecayedT, Types...>::value + 1;
        ::new (storage_) DecayedT(std::forward<T>(value));
    }

    ArtifactVariant(ArtifactVariant&& other) noexcept {
        if (other.typeIndex_ != 0) {
            moveFrom(std::move(other));
        }
    }

    ArtifactVariant(const ArtifactVariant& other) {
        if (other.typeIndex_ != 0) {
            copyFrom(other);
        }
    }

    ArtifactVariant& operator=(ArtifactVariant&& other) noexcept {
        if (this != &other) {
            destroy();
            if (other.typeIndex_ != 0) {
                moveFrom(std::move(other));
            }
        }
        return *this;
    }

    ArtifactVariant& operator=(const ArtifactVariant& other) {
        if (this != &other) {
            destroy();
            if (other.typeIndex_ != 0) {
                copyFrom(other);
            }
        }
        return *this;
    }

    ~ArtifactVariant() { destroy(); }

    size_t index() const noexcept { return typeIndex_ > 0 ? typeIndex_ - 1 : 0; }
    bool isSet() const noexcept { return typeIndex_ != 0; }
    explicit operator bool() const noexcept { return isSet(); }

    template<typename T>
    bool holdsAlternative() const noexcept {
        return typeIndex_ == detail::TypeIndexImpl<0, T, Types...>::value + 1;
    }

    template<typename T>
    T* getIf() noexcept {
        if (holdsAlternative<T>()) {
            return static_cast<T*>(ptr());
        }
        return nullptr;
    }

    template<typename T>
    const T* getIf() const noexcept {
        if (holdsAlternative<T>()) {
            return static_cast<const T*>(ptr());
        }
        return nullptr;
    }

    template<typename T>
    T& get() {
        return *getIf<T>();
    }

    template<typename T>
    const T& get() const {
        return *getIf<T>();
    }

    void swap(ArtifactVariant& other) noexcept {
        ArtifactVariant tmp(std::move(other));
        other = std::move(*this);
        *this = std::move(tmp);
    }
};

} // namespace ArtifactCore
