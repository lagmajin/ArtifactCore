module;

#include <atomic>
#include <type_traits>

export module Core.ArtifactAtomic;

namespace ArtifactCore {

export
template<typename T>
class ArtifactAtomic {
public:
    static_assert(std::is_trivially_copyable_v<T>, "ArtifactAtomic requires trivially copyable type");
    
    ArtifactAtomic() noexcept : value_{} {}
    
    explicit ArtifactAtomic(T initialValue) noexcept : value_{initialValue} {}
    
    ArtifactAtomic(const ArtifactAtomic&) = delete;
    ArtifactAtomic& operator=(const ArtifactAtomic&) = delete;
    
    T load(std::memory_order mo = std::memory_order_seq_cst) const noexcept {
        return value_.load(mo);
    }
    
    void store(T newValue, std::memory_order mo = std::memory_order_seq_cst) noexcept {
        value_.store(newValue, mo);
    }
    
    T exchange(T newValue, std::memory_order mo = std::memory_order_seq_cst) noexcept {
        return value_.exchange(newValue, mo);
    }
    
    bool compare_exchange_strong(T& expected, T desired, 
                                  std::memory_order success = std::memory_order_seq_cst,
                                  std::memory_order failure = std::memory_order_seq_cst) noexcept {
        return value_.compare_exchange_strong(expected, desired, success, failure);
    }
    
    bool compare_exchange_weak(T& expected, T desired,
                              std::memory_order success = std::memory_order_seq_cst,
                              std::memory_order failure = std::memory_order_seq_cst) noexcept {
        return value_.compare_exchange_weak(expected, desired, success, failure);
    }
    
    T fetch_add(T operand, std::memory_order mo = std::memory_order_seq_cst) noexcept {
        return value_.fetch_add(operand, mo);
    }
    
    T fetch_sub(T operand, std::memory_order mo = std::memory_order_seq_cst) noexcept {
        return value_.fetch_sub(operand, mo);
    }
    
    T fetch_and(T operand, std::memory_order mo = std::memory_order_seq_cst) noexcept {
        return value_.fetch_and(operand, mo);
    }
    
    T fetch_or(T operand, std::memory_order mo = std::memory_order_seq_cst) noexcept {
        return value_.fetch_or(operand, mo);
    }
    
    T fetch_xor(T operand, std::memory_order mo = std::memory_order_seq_cst) noexcept {
        return value_.fetch_xor(operand, mo);
    }
    
    void setContended(bool contended) noexcept { contended_ = contended; }
    bool isContended() const noexcept { return contended_.load(); }
    
private:
    mutable std::atomic<T> value_;
    std::atomic<bool> contended_{false};
};

} // namespace ArtifactCore
