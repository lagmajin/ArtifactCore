module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <type_traits>

export module Memory.ArtifactArray;

namespace ArtifactCore {

template<typename T, typename Allocator = std::allocator<T>>
class ArtifactArray {
public:
    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = pointer;
    using const_iterator = const_pointer;
    
    ArtifactArray() noexcept : size_(0), capacity_(0), data_(nullptr) {}
    
    explicit ArtifactArray(size_type count) 
        : size_(count), capacity_(count), data_(allocator_.allocate(count)) {
        for (size_type i = 0; i < count; ++i) {
            allocator_.construct(&data_[i]);
        }
    }
    
    ArtifactArray(size_type count, const T& value)
        : size_(count), capacity_(count), data_(allocator_.allocate(count)) {
        for (size_type i = 0; i < count; ++i) {
            allocator_.construct(&data_[i], value);
        }
    }
    
    ArtifactArray(std::initializer_list<T> init)
        : size_(init.size()), capacity_(init.size()), data_(allocator_.allocate(init.size())) {
        size_type i = 0;
        for (const auto& val : init) {
            allocator_.construct(&data_[i++], val);
        }
    }
    
    ArtifactArray(const ArtifactArray& other)
        : size_(other.size_), capacity_(other.size_), data_(allocator_.allocate(other.size_)) {
        for (size_type i = 0; i < size_; ++i) {
            allocator_.construct(&data_[i], other.data_[i]);
        }
    }
    
    ArtifactArray(ArtifactArray&& other) noexcept
        : size_(other.size_), capacity_(other.capacity_), data_(other.data_) {
        other.size_ = 0;
        other.capacity_ = 0;
        other.data_ = nullptr;
    }
    
    ArtifactArray& operator=(const ArtifactArray& other) {
        if (this != &other) {
            clear();
            deallocate();
            
            size_ = other.size_;
            capacity_ = other.size_;
            data_ = allocator_.allocate(other.size_);
            
            for (size_type i = 0; i < size_; ++i) {
                allocator_.construct(&data_[i], other.data_[i]);
            }
        }
        return *this;
    }
    
    ArtifactArray& operator=(ArtifactArray&& other) noexcept {
        if (this != &other) {
            clear();
            deallocate();
            
            size_ = other.size_;
            capacity_ = other.capacity_;
            data_ = other.data_;
            
            other.size_ = 0;
            other.capacity_ = 0;
            other.data_ = nullptr;
        }
        return *this;
    }
    
    ~ArtifactArray() {
        clear();
        deallocate();
    }
    
    reference operator[](size_type pos) { return data_[pos]; }
    const_reference operator[](size_type pos) const { return data_[pos]; }
    
    reference at(size_type pos) {
        if (pos >= size_) throw std::out_of_range("ArtifactArray::at");
        return data_[pos];
    }
    
    const_reference at(size_type pos) const {
        if (pos >= size_) throw std::out_of_range("ArtifactArray::at");
        return data_[pos];
    }
    
    reference front() { return data_[0]; }
    const_reference front() const { return data_[0]; }
    
    reference back() { return data_[size_ - 1]; }
    const_reference back() const { return data_[size_ - 1]; }
    
    pointer data() noexcept { return data_; }
    const_pointer data() const noexcept { return data_; }
    
    iterator begin() noexcept { return data_; }
    const_iterator begin() const noexcept { return data_; }
    const_iterator cbegin() const noexcept { return data_; }
    
    iterator end() noexcept { return data_ + size_; }
    const_iterator end() const noexcept { return data_ + size_; }
    const_iterator cend() const noexcept { return data_ + size_; }
    
    bool empty() const noexcept { return size_ == 0; }
    size_type size() const noexcept { return size_; }
    size_type capacity() const noexcept { return capacity_; }
    
    void reserve(size_type newCapacity) {
        if (newCapacity > capacity_) {
            reallocate(newCapacity);
        }
    }
    
    void shrink_to_fit() {
        if (capacity_ > size_) {
            reallocate(size_);
        }
    }
    
    void clear() noexcept {
        for (size_type i = 0; i < size_; ++i) {
            allocator_.destroy(&data_[i]);
        }
        size_ = 0;
    }
    
    void push_back(const T& value) {
        emplace_back(value);
    }
    
    void push_back(T&& value) {
        emplace_back(std::move(value));
    }
    
    template<typename... Args>
    reference emplace_back(Args&&... args) {
        if (size_ >= capacity_) {
            grow();
        }
        allocator_.construct(&data_[size_], std::forward<Args>(args)...);
        return data_[size_++];
    }
    
    void pop_back() {
        if (size_ > 0) {
            allocator_.destroy(&data_[--size_]);
        }
    }
    
    iterator insert(iterator pos, const T& value) {
        return insert_impl(pos, value);
    }
    
    iterator insert(iterator pos, T&& value) {
        return insert_impl(pos, std::move(value));
    }
    
    iterator erase(iterator pos) {
        if (pos >= data_ && pos < data_ + size_) {
            allocator_.destroy(pos);
            std::move(pos + 1, data_ + size_, pos);
            --size_;
            return pos;
        }
        return end();
    }
    
    iterator erase(iterator first, iterator last) {
        if (first >= data_ && last <= data_ + size_) {
            for (iterator it = first; it != last; ++it) {
                allocator_.destroy(it);
            }
            const difference_type shift = last - first;
            std::move(last, data_ + size_, first);
            size_ -= shift;
            return first;
        }
        return end();
    }
    
    void resize(size_type newSize) {
        if (newSize > size_) {
            if (newSize > capacity_) {
                reallocate(newSize);
            }
            for (size_type i = size_; i < newSize; ++i) {
                allocator_.construct(&data_[i]);
            }
        } else if (newSize < size_) {
            for (size_type i = newSize; i < size_; ++i) {
                allocator_.destroy(&data_[i]);
            }
        }
        size_ = newSize;
    }
    
    void resize(size_type newSize, const T& value) {
        if (newSize > size_) {
            if (newSize > capacity_) {
                reallocate(newSize);
            }
            for (size_type i = size_; i < newSize; ++i) {
                allocator_.construct(&data_[i], value);
            }
        } else if (newSize < size_) {
            for (size_type i = newSize; i < size_; ++i) {
                allocator_.destroy(&data_[i]);
            }
        }
        size_ = newSize;
    }
    
    void swap(ArtifactArray& other) noexcept {
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
        std::swap(data_, other.data_);
    }
    
private:
    Allocator allocator_;
    size_type size_;
    size_type capacity_;
    pointer data_;
    
    void grow() {
        const size_type newCapacity = capacity_ == 0 ? 8 : capacity_ * 2;
        reallocate(newCapacity);
    }
    
    void reallocate(size_type newCapacity) {
        pointer newData = allocator_.allocate(newCapacity);
        for (size_type i = 0; i < size_; ++i) {
            allocator_.construct(&newData[i], std::move(data_[i]));
            allocator_.destroy(&data_[i]);
        }
        if (data_) {
            allocator_.deallocate(data_, capacity_);
        }
        data_ = newData;
        capacity_ = newCapacity;
    }
    
    void deallocate() {
        if (data_) {
            allocator_.deallocate(data_, capacity_);
            data_ = nullptr;
        }
    }
    
    template<typename U>
    iterator insert_impl(iterator pos, U&& value) {
        const difference_type index = pos - data_;
        if (size_ >= capacity_) {
            grow();
        }
        pos = data_ + index;
        std::move_backward(pos, data_ + size_, data_ + size_ + 1);
        allocator_.destroy(pos);
        allocator_.construct(pos, std::forward<U>(value));
        ++size_;
        return pos;
    }
};

template<typename T, typename Alloc>
void swap(ArtifactArray<T, Alloc>& a, ArtifactArray<T, Alloc>& b) noexcept {
    a.swap(b);
}

} // namespace ArtifactCore
