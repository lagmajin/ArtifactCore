module;
#include <cstddef>

export module Core.ArtifactSpan;

export namespace ArtifactCore {

template<typename T>
class ArtifactSpan {
public:
    constexpr ArtifactSpan() noexcept : data_(nullptr), size_(0) {}

    constexpr ArtifactSpan(T* data, size_t size) noexcept : data_(data), size_(size) {}

    template<typename Container>
    constexpr ArtifactSpan(Container& c) noexcept : data_(c.data()), size_(c.size()) {}

    constexpr T* data() const noexcept { return data_; }
    constexpr size_t size() const noexcept { return size_; }
    constexpr bool isEmpty() const noexcept { return size_ == 0; }

    constexpr T& operator[](size_t i) const noexcept { return data_[i]; }
    constexpr T& front() const noexcept { return data_[0]; }
    constexpr T& back() const noexcept { return data_[size_ - 1]; }

    constexpr T* begin() const noexcept { return data_; }
    constexpr T* end() const noexcept { return data_ + size_; }

    constexpr ArtifactSpan subspan(size_t offset, size_t count) const noexcept {
        return ArtifactSpan(data_ + offset, count);
    }
    constexpr ArtifactSpan first(size_t count) const noexcept { return subspan(0, count); }
    constexpr ArtifactSpan last(size_t count) const noexcept { return subspan(size_ - count, count); }

private:
    T* data_;
    size_t size_;
};

} // namespace ArtifactCore
