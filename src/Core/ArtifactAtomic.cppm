module;

#include <atomic>
#include <type_traits>

export module Core.ArtifactAtomic;

namespace ArtifactCore {

template<typename T>
class ArtifactAtomic {
public:
    static_assert(std::is_trivially_copyable_v<T>, "ArtifactAtomic requires trivially copyable type");
    
    ArtifactAtomic() noexcept : value_{} {}
    
    explicit ArtifactAtomic(T initialValue) noexcept : value_{initialValue} {}
    
    ArtifactAtomic(const ArtifactAtomic&) = delete;
    ArtifactAtomic& operator=(const ArtifactAtomic&) = delete;
    
    T load(int memoryOrder = std::memory_order_seq_cst) const noexcept {
        return value_.load(static_cast<std::memory_order>(memoryOrder));
    }
    
    void store(T newValue, int memoryOrder = std::memory_order_seq_cst) noexcept {
        value_.store(newValue, static_cast<std::memory_order>(memoryOrder));
    }
    
    T exchange(T newValue, int memoryOrder = std::memory_order_seq_cst) noexcept {
        return value_.exchange(newValue, static_cast<std::memory_order>(memoryOrder));
    }
    
    bool compare_exchange_strong(T& expected, T desired, 
                                  int successOrder = std::memory_order_seq_cst,
                                  int failureOrder = std::memory_order_seq_cst) noexcept {
        return value_.compare_exchange_strong(
            expected, desired,
            static_cast<std::memory_order>(successOrder),
            static_cast<std::memory_order>(failureOrder));
    }
    
    bool compare_exchange_weak(T& expected, T desired,
                              int successOrder = std::memory_order_seq_cst,
                              int failureOrder = std::memory_order_seq_cst) noexcept {
        return value_.compare_exchange_weak(
            expected, desired,
            static_cast<std::memory_order>(successOrder),
            static_cast<std::memory_order>(failureOrder));
    }
    
    T fetch_add(T operand, int memoryOrder = std::memory_order_seq_cst) noexcept {
        return value_.fetch_add(operand, static_cast<std::memory_order>(memoryOrder));
    }
    
    T fetch_sub(T operand, int memoryOrder = std::memory_order_seq_cst) noexcept {
        return value_.fetch_sub(operand, static_cast<std::memory_order>(memoryOrder));
    }
    
    T fetch_and(T operand, int memoryOrder = std::memory_order_seq_cst) noexcept {
        return value_.fetch_and(operand, static_cast<std::memory_order>(memoryOrder));
    }
    
    T fetch_or(T operand, int memoryOrder = std::memory_order_seq_cst) noexcept {
        return value_.fetch_or(operand, static_cast<std::memory_order>(memoryOrder));
    }
    
    T fetch_xor(T operand, int memoryOrder = std::memory_order_seq_cst) noexcept {
        return value_.fetch_xor(operand, static_cast<std::memory_order>(memoryOrder));
    }
    
    void setContended(bool contended) noexcept { contended_ = contended; }
    bool isContended() const noexcept { return contended_.load(); }
    
private:
    mutable std::atomic<T> value_;
    std::atomic<bool> contended_{false};
};

} // namespace ArtifactCore
