module;
#include <cstddef>
#include <cstdint>
#include <cassert>
#include <utility>
#include <new>
#include <initializer_list>
#include <cstring>

export module Core.ArtifactArray;

import Core.ArtifactOptional;

export namespace ArtifactCore {

template <typename T>
class Array {
public:
    // --- Construction ---
    Array() = default;
    Array(std::initializer_list<T> init) { reserve(init.size()); for (const auto& v : init) append(v); }
    explicit Array(size_t size, const T& fillValue) { resize(size); fill(fillValue); }
    Array(const Array& other) { copyFrom(other); }
    Array(Array&& other) noexcept : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
        other.data_ = nullptr; other.size_ = 0; other.capacity_ = 0;
    }
    ~Array() { destroyAll(); release(); }
    Array& operator=(const Array& other) { if (this == &other) return *this; Array tmp(other); swap(tmp); return *this; }
    Array& operator=(Array&& other) noexcept {
        if (this == &other) return *this; destroyAll(); release();
        data_ = other.data_; size_ = other.size_; capacity_ = other.capacity_;
        other.data_ = nullptr; other.size_ = 0; other.capacity_ = 0;
        return *this;
    }

    // --- Append / Prepend ---
    void append(const T& v) { ensureCapacity(size_ + 1); new (data_ + size_) T(v); ++size_; }
    void append(T&& v) { ensureCapacity(size_ + 1); new (data_ + size_) T(std::move(v)); ++size_; }
    Array& operator<<(const T& v) { append(v); return *this; }  // stream style
    Array& operator<<(T&& v) { append(std::move(v)); return *this; }
    void prepend(const T& v) { insert(0, v); }
    void appendAll(const Array& other) { for (const auto& v : other) append(v); }
    Array operator+(const Array& other) const { Array r(*this); r.appendAll(other); return r; }

    // --- Insert / Replace ---
    void insert(size_t index, const T& v) {
        if (index > size_) return;
        ensureCapacity(size_ + 1);
        for (size_t i = size_; i > index; --i) { new (data_ + i) T(std::move(data_[i - 1])); data_[i - 1].~T(); }
        new (data_ + index) T(v);
        ++size_;
    }
    void insert(size_t index, T&& v) {
        if (index > size_) return;
        ensureCapacity(size_ + 1);
        for (size_t i = size_; i > index; --i) { new (data_ + i) T(std::move(data_[i - 1])); data_[i - 1].~T(); }
        new (data_ + index) T(std::move(v));
        ++size_;
    }
    void replace(size_t index, const T& v) {
        if (index >= size_) return;
        data_[index].~T();
        new (data_ + index) T(v);
    }

    // --- Safe access ---
    Optional<T&> at(size_t i) { return (i < size_) ? Optional<T&>(data_[i]) : Nullopt; }
    Optional<const T&> at(size_t i) const { return (i < size_) ? Optional<const T&>(data_[i]) : Nullopt; }
    T value(size_t i, const T& fallback) const { return (i < size_) ? data_[i] : fallback; }
    Optional<T&> first() { return at(0); }
    Optional<T&> last() { return size_ > 0 ? Optional<T&>(data_[size_ - 1]) : Nullopt; }
    Optional<const T&> first() const { return at(0); }
    Optional<const T&> last() const { return size_ > 0 ? Optional<const T&>(data_[size_ - 1]) : Nullopt; }

    // --- Remove ---
    void removeAt(size_t index) {
        if (index >= size_) return;
        data_[index].~T();
        for (size_t i = index + 1; i < size_; ++i) { new (data_ + i - 1) T(std::move(data_[i])); data_[i].~T(); }
        --size_;
    }
    void removeFirst() { removeAt(0); }
    void removeLast() { if (size_ == 0) return; data_[size_ - 1].~T(); --size_; }
    T takeAt(size_t index) { T v(std::move(data_[index])); removeAt(index); return v; }
    T takeFirst() { return takeAt(0); }
    T takeLast() { T v(std::move(data_[size_ - 1])); removeLast(); return v; }
    void removeAll() { destroyAll(); size_ = 0; }
    void removeOne(const T& v) { auto idx = indexOf(v); if (idx >= 0) removeAt(static_cast<size_t>(idx)); }
    void removeAll(const T& v) {
        size_t w = 0;
        for (size_t r = 0; r < size_; ++r) {
            if (data_[r] == v) { data_[r].~T(); }
            else { if (w != r) { new (data_ + w) T(std::move(data_[r])); data_[r].~T(); } ++w; }
        }
        size_ = w;
    }

    // --- Search ---
    Optional<T&> find(const T& v) {
        for (size_t i = 0; i < size_; ++i) if (data_[i] == v) return Optional<T&>(data_[i]);
        return Nullopt;
    }
    int indexOf(const T& v) const {
        for (size_t i = 0; i < size_; ++i) if (data_[i] == v) return static_cast<int>(i);
        return -1;
    }
    int lastIndexOf(const T& v) const {
        for (size_t i = size_; i > 0; --i) if (data_[i - 1] == v) return static_cast<int>(i - 1);
        return -1;
    }
    bool contains(const T& v) const { return indexOf(v) >= 0; }
    int count(const T& v) const { int n = 0; for (const auto& e : *this) if (e == v) ++n; return n; }

    // --- Sub-array ---
    Array mid(size_t pos, size_t len = ~0ULL) const {
        if (pos >= size_) return Array();
        if (len > size_ - pos) len = size_ - pos;
        Array result; result.reserve(len);
        for (size_t i = 0; i < len; ++i) result.append(data_[pos + i]);
        return result;
    }
    Array left(size_t len) const { return mid(0, len); }
    Array right(size_t len) const { return len >= size_ ? *this : mid(size_ - len, len); }
    Array reversed() const { Array r; r.reserve(size_); for (size_t i = size_; i > 0; --i) r.append(data_[i - 1]); return r; }

    // --- Fill / Resize ---
    void fill(const T& v) { for (size_t i = 0; i < size_; ++i) data_[i] = v; }
    void resize(size_t n) {
        if (n < size_) { while (size_ > n) removeLast(); }
        else if (n > size_) { ensureCapacity(n); while (size_ < n) { new (data_ + size_) T(); ++size_; } }
    }
    void resize(size_t n, const T& fillValue) {
        if (n < size_) { while (size_ > n) removeLast(); }
        else if (n > size_) { ensureCapacity(n); while (size_ < n) { new (data_ + size_) T(fillValue); ++size_; } }
    }

    // --- Capacity ---
    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }
    bool isEmpty() const { return size_ == 0; }
    void reserve(size_t n) {
        if (n <= capacity_) return;
        T* nd = static_cast<T*>(::operator new(n * sizeof(T)));
        for (size_t i = 0; i < size_; ++i) { new (nd + i) T(std::move(data_[i])); data_[i].~T(); }
        ::operator delete(data_);
        data_ = nd; capacity_ = n;
    }
    void squeeze() { if (size_ < capacity_) reserve(size_); }

    // --- Comparison ---
    bool operator==(const Array& other) const {
        if (size_ != other.size_) return false;
        for (size_t i = 0; i < size_; ++i) if (!(data_[i] == other.data_[i])) return false;
        return true;
    }
    bool operator!=(const Array& other) const { return !(*this == other); }
    bool startsWith(const Array& prefix) const {
        if (prefix.size_ > size_) return false;
        for (size_t i = 0; i < prefix.size_; ++i) if (!(data_[i] == prefix.data_[i])) return false;
        return true;
    }
    bool endsWith(const Array& suffix) const {
        if (suffix.size_ > size_) return false;
        size_t off = size_ - suffix.size_;
        for (size_t i = 0; i < suffix.size_; ++i) if (!(data_[off + i] == suffix.data_[i])) return false;
        return true;
    }

    // --- Low-level access (use carefully) ---
    T* data() { return data_; }
    const T* data() const { return data_; }
    const T* constData() const { return data_; }

    // --- Iteration ---
    T* begin() { return data_; }
    T* end() { return data_ + size_; }
    const T* begin() const { return data_; }
    const T* end() const { return data_ + size_; }
    const T* constBegin() const { return data_; }
    const T* constEnd() const { return data_ + size_; }

    void swap(Array& o) noexcept {
        T* td = o.data_; o.data_ = data_; data_ = td;
        size_t ts = o.size_; o.size_ = size_; size_ = ts;
        size_t tc = o.capacity_; o.capacity_ = capacity_; capacity_ = tc;
    }

private:
    T* data_ = nullptr;
    size_t size_ = 0;
    size_t capacity_ = 0;

    void copyFrom(const Array& o) {
        if (o.size_ == 0) return;
        reserve(o.size_);
        for (size_t i = 0; i < o.size_; ++i) new (data_ + i) T(o.data_[i]);
        size_ = o.size_;
    }
    void ensureCapacity(size_t n) {
        if (n <= capacity_) return;
        size_t nc = capacity_ == 0 ? 4 : capacity_ * 2;
        if (nc < n) nc = n;
        reserve(nc);
    }
    void destroyAll() { for (size_t i = 0; i < size_; ++i) data_[i].~T(); }
    void release() { ::operator delete(data_); data_ = nullptr; capacity_ = 0; }
};

template <typename T>
using ArtifactArray = Array<T>;

} // namespace ArtifactCore
