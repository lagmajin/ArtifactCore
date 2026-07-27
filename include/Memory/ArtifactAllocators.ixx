module;
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <utility>
#include <vector>

export module Memory.ArtifactAllocators;

export namespace ArtifactCore {

struct AllocationMetrics {
    std::size_t allocatedBytes = 0;
    std::size_t outstandingBytes = 0;
    std::size_t peakOutstandingBytes = 0;
    std::size_t allocationCount = 0;
};

// Counts allocations made by the wrapped upstream resource. It intentionally
// does not own the upstream resource and is suitable for diagnostics only.
class CountingMemoryResource final : public std::pmr::memory_resource {
public:
    explicit CountingMemoryResource(
        std::pmr::memory_resource* upstream = std::pmr::get_default_resource()) noexcept
        : upstream_(upstream != nullptr ? upstream : std::pmr::get_default_resource()) {}

    [[nodiscard]] AllocationMetrics metrics() const noexcept {
        return {
            allocatedBytes_.load(std::memory_order_relaxed),
            outstandingBytes_.load(std::memory_order_relaxed),
            peakOutstandingBytes_.load(std::memory_order_relaxed),
            allocationCount_.load(std::memory_order_relaxed)
        };
    }

    void resetMetrics() noexcept {
        allocatedBytes_.store(0, std::memory_order_relaxed);
        outstandingBytes_.store(0, std::memory_order_relaxed);
        peakOutstandingBytes_.store(0, std::memory_order_relaxed);
        allocationCount_.store(0, std::memory_order_relaxed);
    }

protected:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        void* allocation = upstream_->allocate(bytes, alignment);
        allocatedBytes_.fetch_add(bytes, std::memory_order_relaxed);
        allocationCount_.fetch_add(1, std::memory_order_relaxed);
        const std::size_t outstanding =
            outstandingBytes_.fetch_add(bytes, std::memory_order_relaxed) + bytes;
        std::size_t peak = peakOutstandingBytes_.load(std::memory_order_relaxed);
        while (peak < outstanding &&
               !peakOutstandingBytes_.compare_exchange_weak(
                   peak, outstanding, std::memory_order_relaxed, std::memory_order_relaxed)) {
        }
        return allocation;
    }

    void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override {
        upstream_->deallocate(pointer, bytes, alignment);
        outstandingBytes_.fetch_sub(bytes, std::memory_order_relaxed);
    }

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }

private:
    std::pmr::memory_resource* upstream_;
    std::atomic<std::size_t> allocatedBytes_{0};
    std::atomic<std::size_t> outstandingBytes_{0};
    std::atomic<std::size_t> peakOutstandingBytes_{0};
    std::atomic<std::size_t> allocationCount_{0};
};

// A reset-at-once allocator for render passes and other bounded lifetimes.
// Individual deallocation is intentionally ignored by the monotonic resource.
class FrameAllocator final {
public:
    explicit FrameAllocator(
        std::size_t initialBytes = 256 * 1024,
        std::pmr::memory_resource* upstream = std::pmr::get_default_resource())
        : initialStorage_(initialBytes),
          upstream_(upstream),
          resource_(initialStorage_.data(), initialStorage_.size(), &upstream_) {}

    [[nodiscard]] std::pmr::memory_resource* resource() noexcept { return &resource_; }
    [[nodiscard]] const CountingMemoryResource& upstreamMetrics() const noexcept { return upstream_; }

    void reset() { resource_.release(); }

private:
    std::vector<std::byte> initialStorage_;
    CountingMemoryResource upstream_;
    std::pmr::monotonic_buffer_resource resource_;
};

// Thread-confined, reusable allocator for effect instances and worker-local
// scratch. It must not be used concurrently from more than one thread.
class TaskAllocator final {
public:
    explicit TaskAllocator(std::pmr::memory_resource* upstream = std::pmr::get_default_resource())
        : upstream_(upstream), resource_(&upstream_) {}

    [[nodiscard]] std::pmr::memory_resource* resource() noexcept { return &resource_; }
    [[nodiscard]] const CountingMemoryResource& upstreamMetrics() const noexcept { return upstream_; }

    void release() { resource_.release(); }

private:
    CountingMemoryResource upstream_;
    std::pmr::unsynchronized_pool_resource resource_;
};

// Reusable allocator for genuinely shared allocation sites. Prefer
// TaskAllocator whenever ownership can remain thread-confined.
class ConcurrentAllocator final {
public:
    explicit ConcurrentAllocator(std::pmr::memory_resource* upstream = std::pmr::get_default_resource())
        : upstream_(upstream), resource_(&upstream_) {}

    [[nodiscard]] std::pmr::memory_resource* resource() noexcept { return &resource_; }
    [[nodiscard]] const CountingMemoryResource& upstreamMetrics() const noexcept { return upstream_; }

    void release() { resource_.release(); }

private:
    CountingMemoryResource upstream_;
    std::pmr::synchronized_pool_resource resource_;
};

template <typename T>
using PmrVector = std::pmr::vector<T>;

} // namespace ArtifactCore
