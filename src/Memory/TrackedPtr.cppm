module;

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <typeinfo>
#include <type_traits>
#include <utility>
#include <vector>

export module Memory.TrackedPtr;

namespace ArtifactCore {

namespace Memory {

using TrackedDeleter = void(*)(void*);

template<typename T>
inline void defaultTrackedDelete(T* ptr);

class SmallIdSet {
public:
    bool contains(uint64_t value) const noexcept {
        for (uint64_t entry : values_) {
            if (entry == value) {
                return true;
            }
        }
        return false;
    }

    bool insert(uint64_t value) {
        if (contains(value)) {
            return false;
        }
        values_.push_back(value);
        return true;
    }

    void erase(uint64_t value) {
        for (size_t index = 0; index < values_.size(); ++index) {
            if (values_[index] == value) {
                values_[index] = values_.back();
                values_.pop_back();
                return;
            }
        }
    }

    const std::vector<uint64_t>& values() const noexcept {
        return values_;
    }

private:
    std::vector<uint64_t> values_;
};

struct TrackedPtrControlBlock {
    std::atomic<int32_t> strongRefCount{1};
    std::atomic<int32_t> weakRefCount{0};
    void* ptr{nullptr};
    TrackedDeleter deleter{nullptr};
    
    // Cycle detection
    uint64_t allocationId{0};
    SmallIdSet cycleNeighbors;
    mutable std::mutex cycleMutex;
    
    // Debug metadata
    std::string typeName;
    std::string allocationSite;
    std::vector<std::string> allocationStack;
    
    TrackedPtrControlBlock() = default;
    TrackedPtrControlBlock(void* p, TrackedDeleter d, uint64_t id)
        : ptr(p), deleter(d), allocationId(id) {}
};

class TrackedPtrRegistry {
public:
    static TrackedPtrRegistry& instance() {
        static TrackedPtrRegistry registry;
        return registry;
    }
    
    uint64_t registerPointer(TrackedPtrControlBlock* block) {
        std::lock_guard<std::mutex> lock(mutex_);
        const uint64_t id = ++nextId_;
        if (id >= blocks_.size()) {
            blocks_.resize(static_cast<size_t>(id) + 1, nullptr);
        }
        blocks_[static_cast<size_t>(id)] = block;
        block->allocationId = id;
        return id;
    }
    
    void unregisterPointer(uint64_t id) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (id < blocks_.size()) {
            blocks_[static_cast<size_t>(id)] = nullptr;
        }
    }
    
    TrackedPtrControlBlock* controlBlock(uint64_t id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (id >= blocks_.size()) {
            return nullptr;
        }
        return blocks_[static_cast<size_t>(id)];
    }
    
    std::vector<uint64_t> detectCycles() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<uint64_t> cycleParticipants;
        
        for (uint64_t id = 1; id < blocks_.size(); ++id) {
            const auto* block = blocks_[static_cast<size_t>(id)];
            if (block && block->strongRefCount.load() > 0) {
                if (isPartOfCycle(block, id)) {
                    cycleParticipants.push_back(id);
                }
            }
        }
        return cycleParticipants;
    }
    
private:
    mutable std::mutex mutex_;
    uint64_t nextId_{0};
    std::vector<TrackedPtrControlBlock*> blocks_;
    
    bool isPartOfCycle(const TrackedPtrControlBlock* block, uint64_t id) const {
        if (!block) return false;
        
        SmallIdSet visited;
        SmallIdSet recStack;
        return hasCycleDFS(block, id, visited, recStack);
    }
    
    bool hasCycleDFS(const TrackedPtrControlBlock* block, uint64_t id,
                     SmallIdSet& visited,
                     SmallIdSet& recStack) const {
        if (recStack.contains(id)) return true;
        if (visited.contains(id)) return false;
        
        visited.insert(id);
        recStack.insert(id);
        
        std::lock_guard<std::mutex> lock(block->cycleMutex);
        for (uint64_t neighborId : block->cycleNeighbors.values()) {
            TrackedPtrControlBlock* neighbor = controlBlock(neighborId);
            if (neighbor && hasCycleDFS(neighbor, neighborId, visited, recStack)) {
                return true;
            }
        }
        
        recStack.erase(id);
        return false;
    }
};

template<typename T>
class TrackedPtr {
public:
    template<typename U>
    friend class TrackedWeakPtr;

    TrackedPtr() noexcept : controlBlock_(nullptr), ptr_(nullptr) {}
    
    explicit TrackedPtr(T* ptr) : controlBlock_(nullptr), ptr_(nullptr) {
        if (ptr) {
            controlBlock_ = new TrackedPtrControlBlock(ptr, &TrackedPtr::defaultDelete, 0);
            controlBlock_->typeName = typeid(T).name();
            ptr_ = ptr;
            Memory::TrackedPtrRegistry::instance().registerPointer(controlBlock_);
        }
    }
    
    TrackedPtr(T* ptr, TrackedDeleter deleter) : controlBlock_(nullptr), ptr_(nullptr) {
        if (ptr) {
            controlBlock_ = new TrackedPtrControlBlock(ptr, deleter, 0);
            controlBlock_->typeName = typeid(T).name();
            ptr_ = ptr;
            Memory::TrackedPtrRegistry::instance().registerPointer(controlBlock_);
        }
    }
    
    TrackedPtr(std::nullptr_t) noexcept : controlBlock_(nullptr), ptr_(nullptr) {}
    
    TrackedPtr(const TrackedPtr& other) noexcept 
        : controlBlock_(other.controlBlock_), ptr_(other.ptr_) {
        if (controlBlock_) {
            controlBlock_->strongRefCount.fetch_add(1, std::memory_order_relaxed);
        }
    }
    
    TrackedPtr(TrackedPtr&& other) noexcept 
        : controlBlock_(other.controlBlock_), ptr_(other.ptr_) {
        other.controlBlock_ = nullptr;
        other.ptr_ = nullptr;
    }

    TrackedPtr(TrackedPtrControlBlock* controlBlock, T* ptr) noexcept
        : controlBlock_(controlBlock), ptr_(ptr) {}
    
    template<typename U>
    TrackedPtr(const TrackedPtr<U>& other, 
               typename std::enable_if<std::is_convertible<U*, T*>::value>::type* = nullptr)
        : controlBlock_(other.controlBlock_), ptr_(other.ptr_) {
        if (controlBlock_) {
            controlBlock_->strongRefCount.fetch_add(1, std::memory_order_relaxed);
        }
    }
    
    TrackedPtr& operator=(const TrackedPtr& other) noexcept {
        if (this != &other) {
            reset();
            controlBlock_ = other.controlBlock_;
            ptr_ = other.ptr_;
            if (controlBlock_) {
                controlBlock_->strongRefCount.fetch_add(1, std::memory_order_relaxed);
            }
        }
        return *this;
    }
    
    TrackedPtr& operator=(TrackedPtr&& other) noexcept {
        if (this != &other) {
            reset();
            controlBlock_ = other.controlBlock_;
            ptr_ = other.ptr_;
            other.controlBlock_ = nullptr;
            other.ptr_ = nullptr;
        }
        return *this;
    }
    
    TrackedPtr& operator=(std::nullptr_t) noexcept {
        reset();
        return *this;
    }

    static void defaultDelete(void* p) {
        delete static_cast<T*>(p);
    }
    
    ~TrackedPtr() {
        reset();
    }
    
    T* get() const noexcept { return ptr_; }
    T* operator->() const noexcept { return ptr_; }
    T& operator*() const noexcept { return *ptr_; }
    T& operator[](std::ptrdiff_t idx) const noexcept { return ptr_[idx]; }
    
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    
    int32_t useCount() const noexcept {
        return controlBlock_ ? controlBlock_->strongRefCount.load(std::memory_order_acquire) : 0;
    }
    
    int32_t weakRefCount() const noexcept {
        return controlBlock_ ? controlBlock_->weakRefCount.load(std::memory_order_acquire) : 0;
    }
    
    uint64_t allocationId() const noexcept {
        return controlBlock_ ? controlBlock_->allocationId : 0;
    }
    
    void reset() noexcept {
        if (!controlBlock_) return;
        
        const int32_t newRefCount = controlBlock_->strongRefCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
        
        if (newRefCount == 0) {
            if (controlBlock_->deleter && controlBlock_->ptr) {
                controlBlock_->deleter(controlBlock_->ptr);
            }
            
            const uint64_t id = controlBlock_->allocationId;
            const int32_t weakCount = controlBlock_->weakRefCount.load(std::memory_order_acquire);
            
            if (weakCount == 0) {
                TrackedPtrRegistry::instance().unregisterPointer(id);
                delete controlBlock_;
            }
        }
        
        ptr_ = nullptr;
        controlBlock_ = nullptr;
    }
    
    void setAllocationSite(const std::string& site) {
        if (controlBlock_) {
            controlBlock_->allocationSite = site;
        }
    }
    
    void addCycleNeighbor(uint64_t neighborId) {
        if (controlBlock_) {
            std::lock_guard<std::mutex> lock(controlBlock_->cycleMutex);
            controlBlock_->cycleNeighbors.insert(neighborId);
        }
    }
    
private:
    TrackedPtrControlBlock* controlBlock_;
    T* ptr_;
};

template<typename T>
class TrackedWeakPtr {
public:
    TrackedWeakPtr() noexcept : controlBlock_(nullptr) {}
    
    explicit TrackedWeakPtr(const TrackedPtr<T>& other) noexcept
        : controlBlock_(other.controlBlock_) {
        if (controlBlock_) {
            controlBlock_->weakRefCount.fetch_add(1, std::memory_order_relaxed);
        }
    }
    
    TrackedWeakPtr(const TrackedWeakPtr& other) noexcept
        : controlBlock_(other.controlBlock_) {
        if (controlBlock_) {
            controlBlock_->weakRefCount.fetch_add(1, std::memory_order_relaxed);
        }
    }
    
    TrackedWeakPtr(TrackedWeakPtr&& other) noexcept
        : controlBlock_(other.controlBlock_) {
        other.controlBlock_ = nullptr;
    }
    
    TrackedWeakPtr& operator=(TrackedWeakPtr&& other) noexcept {
        if (this != &other) {
            reset();
            controlBlock_ = other.controlBlock_;
            other.controlBlock_ = nullptr;
        }
        return *this;
    }
    
    TrackedWeakPtr& operator=(const TrackedPtr<T>& other) noexcept {
        if (controlBlock_ != other.controlBlock_) {
            reset();
            controlBlock_ = other.controlBlock_;
            if (controlBlock_) {
                controlBlock_->weakRefCount.fetch_add(1, std::memory_order_relaxed);
            }
        }
        return *this;
    }
    
    TrackedWeakPtr& operator=(const TrackedWeakPtr& other) noexcept {
        if (this != &other) {
            reset();
            controlBlock_ = other.controlBlock_;
            if (controlBlock_) {
                controlBlock_->weakRefCount.fetch_add(1, std::memory_order_relaxed);
            }
        }
        return *this;
    }
    
    ~TrackedWeakPtr() {
        reset();
    }
    
    int32_t useCount() const noexcept {
        return controlBlock_ ? controlBlock_->strongRefCount.load(std::memory_order_acquire) : 0;
    }
    
    int32_t weakRefCount() const noexcept {
        return controlBlock_ ? controlBlock_->weakRefCount.load(std::memory_order_acquire) : 0;
    }
    
    void reset() noexcept {
        if (!controlBlock_) return;
        
        const int32_t newWeakCount = controlBlock_->weakRefCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
        
        if (newWeakCount == 0 && controlBlock_->strongRefCount.load() == 0) {
            TrackedPtrRegistry::instance().unregisterPointer(controlBlock_->allocationId);
            delete controlBlock_;
        }
        
        controlBlock_ = nullptr;
    }
    
    TrackedPtr<T> lock() const noexcept {
        if (!controlBlock_) return TrackedPtr<T>();
        
        const int32_t strongCount = controlBlock_->strongRefCount.load(std::memory_order_acquire);
        if (strongCount == 0) return TrackedPtr<T>();

        controlBlock_->strongRefCount.fetch_add(1, std::memory_order_relaxed);
        return TrackedPtr<T>(controlBlock_, static_cast<T*>(controlBlock_->ptr));
    }
    
private:
    TrackedPtrControlBlock* controlBlock_;
};

template<typename T>
class TrackedUniquePtr {
public:
    using element_type = T;
    using deleter_type = void(*)(T*);

    constexpr TrackedUniquePtr() noexcept
        : ptr_(nullptr), deleter_(&defaultTrackedDelete<T>) {}

    constexpr TrackedUniquePtr(std::nullptr_t) noexcept
        : ptr_(nullptr), deleter_(&defaultTrackedDelete<T>) {}

    explicit TrackedUniquePtr(T* ptr) noexcept
        : ptr_(ptr), deleter_(&defaultTrackedDelete<T>) {}

    TrackedUniquePtr(T* ptr, deleter_type deleter) noexcept
        : ptr_(ptr), deleter_(deleter ? deleter : &defaultTrackedDelete<T>) {}

    TrackedUniquePtr(const TrackedUniquePtr&) = delete;
    TrackedUniquePtr& operator=(const TrackedUniquePtr&) = delete;

    TrackedUniquePtr(TrackedUniquePtr&& other) noexcept
        : ptr_(other.ptr_), deleter_(other.deleter_) {
        other.ptr_ = nullptr;
    }

    TrackedUniquePtr& operator=(TrackedUniquePtr&& other) noexcept {
        if (this != &other) {
            reset();
            ptr_ = other.ptr_;
            deleter_ = other.deleter_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    ~TrackedUniquePtr() {
        reset();
    }

    T* get() const noexcept { return ptr_; }
    T& operator*() const noexcept { return *ptr_; }
    T* operator->() const noexcept { return ptr_; }
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

    void swap(TrackedUniquePtr& other) noexcept {
        using std::swap;
        swap(ptr_, other.ptr_);
        swap(deleter_, other.deleter_);
    }

private:
    T* ptr_;
    deleter_type deleter_;
};

template<typename T>
inline void defaultTrackedDelete(T* ptr) {
    delete ptr;
}

template<typename T>
inline TrackedUniquePtr<T> makeTrackedUnique(T* ptr) {
    return TrackedUniquePtr<T>(ptr);
}

template<typename T>
inline TrackedUniquePtr<T> makeTrackedUnique(T* ptr, typename TrackedUniquePtr<T>::deleter_type deleter) {
    return TrackedUniquePtr<T>(ptr, deleter);
}

template<typename T, typename... Args>
inline TrackedUniquePtr<T> makeTrackedUnique(Args&&... args) {
    return TrackedUniquePtr<T>(new T(std::forward<Args>(args)...));
}

inline std::vector<uint64_t> DetectMemoryCycles() {
    return TrackedPtrRegistry::instance().detectCycles();
}

template<typename T>
inline TrackedPtr<T> makeTracked(T* ptr) {
    return TrackedPtr<T>(ptr);
}

template<typename T, typename... Args>
inline TrackedPtr<T> makeTracked(Args&&... args) {
    return TrackedPtr<T>(new T(std::forward<Args>(args)...));
}

} // namespace Memory

template<typename T>
using TrackedPtr = Memory::TrackedPtr<T>;

template<typename T>
using TrackedUniquePtr = Memory::TrackedUniquePtr<T>;

template<typename T>
using TrackedWeakPtr = Memory::TrackedWeakPtr<T>;

} // namespace ArtifactCore
