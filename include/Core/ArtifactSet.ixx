module;
#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>

export module Core.ArtifactSet;

import Core.ArtifactArray;
import Core.ArtifactUtility;

export namespace ArtifactCore {

// Self-contained hash set (chained buckets + insertion-ordered iteration).
// Default hash/equality come from <functional>; supply custom functors for
// user-defined keys.
template <typename T, typename Hasher = std::hash<T>,
          typename KeyEqual = std::equal_to<T>>
class HashSet {
public:
    HashSet() = default;
    HashSet(const HashSet&) = delete; // deep copies omitted; use values()
    HashSet& operator=(const HashSet&) = delete;
    HashSet(HashSet&& other) noexcept
        : buckets_(other.buckets_), bucketCount_(other.bucketCount_),
          head_(other.head_), tail_(other.tail_), size_(other.size_),
          hasher_(other.hasher_), keyEqual_(other.keyEqual_) {
        other.buckets_ = nullptr;
        other.bucketCount_ = 0;
        other.head_ = other.tail_ = nullptr;
        other.size_ = 0;
    }
    HashSet& operator=(HashSet&& other) noexcept {
        if (this == &other) return *this;
        clear();
        deallocateBuckets();
        buckets_ = other.buckets_;
        bucketCount_ = other.bucketCount_;
        head_ = other.head_;
        tail_ = other.tail_;
        size_ = other.size_;
        hasher_ = other.hasher_;
        keyEqual_ = other.keyEqual_;
        other.buckets_ = nullptr;
        other.bucketCount_ = 0;
        other.head_ = other.tail_ = nullptr;
        other.size_ = 0;
        return *this;
    }
    ~HashSet() {
        clear();
        deallocateBuckets();
    }

    // Returns true when the element was newly inserted.
    bool add(const T& item) {
        if (bucketCount_ == 0) allocateBuckets(kInitialBucketCount);
        const std::size_t hash = hasher_(item);
        if (findNode(item, hash)) return false;
        if ((size_ + 1) * 4 >= bucketCount_ * 3) {
            grow();
        }
        const std::size_t index = bucketIndex(hash);
        Node* node = new Node{item, hash, nullptr, buckets_[index], head_};
        buckets_[index] = node;
        if (head_) head_->previousAll = node;
        head_ = node;
        if (!tail_) tail_ = node;
        ++size_;
        return true;
    }

    bool remove(const T& item) {
        if (bucketCount_ == 0) return false;
        const std::size_t hash = hasher_(item);
        const std::size_t index = bucketIndex(hash);
        Node* previous = nullptr;
        Node* node = buckets_[index];
        while (node) {
            if (node->hash == hash && keyEqual_(node->value, item)) {
                unlinkAll(node);
                if (previous) previous->nextBucket = node->nextBucket;
                else buckets_[index] = node->nextBucket;
                delete node;
                --size_;
                return true;
            }
            previous = node;
            node = node->nextBucket;
        }
        return false;
    }

    [[nodiscard]] bool contains(const T& item) const {
        if (bucketCount_ == 0) return false;
        return findNode(item, hasher_(item)) != nullptr;
    }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] bool isEmpty() const noexcept { return size_ == 0; }

    void clear() {
        Node* node = head_;
        while (node) {
            Node* next = node->nextAll;
            delete node;
            node = next;
        }
        head_ = tail_ = nullptr;
        size_ = 0;
        for (std::size_t i = 0; i < bucketCount_; ++i) buckets_[i] = nullptr;
    }

    [[nodiscard]] Array<T> values() const {
        Array<T> result;
        result.reserve(size_);
        for (Node* node = tail_; node; node = node->nextAll) {
            result.append(node->value);
        }
        return result;
    }

    struct Node;

    class Iterator {
    public:
        explicit Iterator(Node* node) noexcept : node_(node) {}
        Iterator& operator++() noexcept { node_ = node_ ? node_->nextAll : nullptr; return *this; }
        [[nodiscard]] const T& operator*() const noexcept { return node_->value; }
        [[nodiscard]] const T* operator->() const noexcept { return &node_->value; }
        [[nodiscard]] bool operator!=(const Iterator& other) const noexcept { return node_ != other.node_; }
        [[nodiscard]] bool operator==(const Iterator& other) const noexcept { return node_ == other.node_; }
    private:
        Node* node_;
    };

    [[nodiscard]] Iterator begin() const noexcept { return Iterator(head_); }
    [[nodiscard]] Iterator end() const noexcept { return Iterator(nullptr); }

private:
    struct Node {
        T value;
        std::size_t hash;
        Node* nextAll;    // newest -> oldest (iteration follows reverse-insertion)
        Node* nextBucket; // per-bucket lookup chain
        Node* previousAll; // oldest-side neighbour for unlinking
    };

    void allocateBuckets(const std::size_t count) {
        bucketCount_ = count;
        buckets_ = new Node*[bucketCount_];
        for (std::size_t i = 0; i < bucketCount_; ++i) buckets_[i] = nullptr;
    }
    void deallocateBuckets() {
        delete[] buckets_;
        buckets_ = nullptr;
        bucketCount_ = 0;
    }
    [[nodiscard]] std::size_t bucketIndex(const std::size_t hash) const noexcept {
        return hash % bucketCount_;
    }

    [[nodiscard]] Node* findNode(const T& item, const std::size_t hash) const {
        if (bucketCount_ == 0) return nullptr;
        Node* node = buckets_[bucketIndex(hash)];
        while (node) {
            if (node->hash == hash && keyEqual_(node->value, item)) return node;
            node = node->nextBucket;
        }
        return nullptr;
    }

    void unlinkAll(Node* node) {
        if (node->previousAll) node->previousAll->nextAll = node->nextAll;
        else head_ = node->nextAll;
        if (node->nextAll) node->nextAll->previousAll = node->previousAll;
        else tail_ = node->previousAll;
    }

    void grow() {
        Node** oldBuckets = buckets_;
        const std::size_t oldCount = bucketCount_;
        allocateBuckets(bucketCount_ == 0 ? kInitialBucketCount : bucketCount_ * 2);
        for (std::size_t i = 0; i < oldCount; ++i) {
            Node* node = oldBuckets[i];
            while (node) {
                Node* next = node->nextBucket;
                const std::size_t index = bucketIndex(node->hash);
                node->nextBucket = buckets_[index];
                buckets_[index] = node;
                node = next;
            }
        }
        delete[] oldBuckets;
    }

    static constexpr std::size_t kInitialBucketCount = 16;

    Node** buckets_ = nullptr;
    std::size_t bucketCount_ = 0;
    Node* head_ = nullptr;
    Node* tail_ = nullptr;
    std::size_t size_ = 0;
    Hasher hasher_{};
    KeyEqual keyEqual_{};
};

// Compatibility alias for the previous QSet-backed wrapper.
template <typename T, typename Hasher = std::hash<T>,
          typename KeyEqual = std::equal_to<T>>
using ArtifactSet = HashSet<T, Hasher, KeyEqual>;

} // namespace ArtifactCore
