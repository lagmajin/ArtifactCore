module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <list>
#include <mutex>
#include <stdexcept>
#include <utility>

export module Core.ArtifactHashMap;

namespace ArtifactCore {

template<typename K, typename V, typename Hasher = std::hash<K>, typename KeyEqual = std::equal_to<K>>
class ArtifactHashMap {
public:
    using key_type = K;
    using mapped_type = V;
    using value_type = std::pair<const K, V>;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using hasher = Hasher;
    using key_equal = KeyEqual;
    
private:
    static constexpr size_type kInitialBucketCount = 16;
    static constexpr float kMaxLoadFactor = 0.75f;
    
    struct Node {
        value_type data;
        size_type hash;
        Node* next;
        
        template<typename... Args>
        explicit Node(size_type h, Args&&... args) 
            : data(std::forward<Args>(args)...), hash(h), next(nullptr) {}
    };
    
    std::unique_ptr<Node*[]> buckets_;
    size_type bucketCount_;
    size_type size_;
    hasher hasher_;
    key_equal keyEqual_;
    
public:
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = typename ArtifactHashMap::value_type;
        using difference_type = typename ArtifactHashMap::difference_type;
        using pointer = typename ArtifactHashMap::value_type*;
        using reference = typename ArtifactHashMap::value_type&;
        
        iterator() noexcept : node_(nullptr), bucketsEnd_(nullptr) {}
        
        iterator(Node* node, Node** current, Node** end) 
            : node_(node), currentBucket_(current), bucketsEnd_(end) {}
        
        reference operator*() const noexcept { return node_->data; }
        pointer operator->() const noexcept { return &node_->data; }
        
        iterator& operator++() {
            ++currentBucket_;
            advanceToNext();
            return *this;
        }
        
        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        
        bool operator==(const iterator& other) const noexcept {
            return node_ == other.node_ && currentBucket_ == other.currentBucket_;
        }
        
        bool operator!=(const iterator& other) const noexcept {
            return !(*this == other);
        }
        
    private:
        Node* node_;
        Node** currentBucket_;
        Node** bucketsEnd_;
        
        void advanceToNext() {
            while (currentBucket_ != bucketsEnd_) {
                if (*currentBucket_ != nullptr) {
                    node_ = *currentBucket_;
                    return;
                }
                ++currentBucket_;
            }
            node_ = nullptr;
        }
        
        friend class ArtifactHashMap;
    };
    
    ArtifactHashMap() : bucketCount_(kInitialBucketCount), size_(0) {
        buckets_ = std::make_unique<Node*[]>(bucketCount_);
        for (size_type i = 0; i < bucketCount_; ++i) {
            buckets_[i] = nullptr;
        }
    }
    
    ArtifactHashMap(size_type bucketCount) : bucketCount_(bucketCount), size_(0) {
        buckets_ = std::make_unique<Node*[]>(bucketCount_);
        for (size_type i = 0; i < bucketCount_; ++i) {
            buckets_[i] = nullptr;
        }
    }
    
    ~ArtifactHashMap() {
        clear();
    }
    
    ArtifactHashMap(const ArtifactHashMap&) = delete;
    ArtifactHashMap& operator=(const ArtifactHashMap&) = delete;
    
    ArtifactHashMap(ArtifactHashMap&& other) noexcept
        : buckets_(std::move(other.buckets_)), 
          bucketCount_(other.bucketCount_), 
          size_(other.size_),
          hasher_(std::move(other.hasher_)),
          keyEqual_(std::move(other.keyEqual_)) {
        other.bucketCount_ = kInitialBucketCount;
        other.size_ = 0;
    }
    
    ArtifactHashMap& operator=(ArtifactHashMap&& other) noexcept {
        clear();
        buckets_ = std::move(other.buckets_);
        bucketCount_ = other.bucketCount_;
        size_ = other.size_;
        hasher_ = std::move(other.hasher_);
        keyEqual_ = std::move(other.keyEqual_);
        other.bucketCount_ = kInitialBucketCount;
        other.size_ = 0;
        return *this;
    }
    
    iterator begin() noexcept {
        Node** bucket = buckets_.get();
        Node** end = bucket + bucketCount_;
        
        Node** first = bucket;
        while (first != end && *first == nullptr) ++first;
        
        iterator it;
        if (first != end) {
            it = iterator(*first, first, end);
        } else {
            it = iterator(nullptr, end, end);
        }
        return it;
    }
    
    iterator end() noexcept {
        return iterator(nullptr, buckets_.get() + bucketCount_, buckets_.get() + bucketCount_);
    }
    
    size_type size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
    size_type bucket_count() const noexcept { return bucketCount_; }
    
    V& operator[](const K& key) {
        return tryEmplace(key).first->second;
    }
    
    V& at(const K& key) {
        Node* node = findNode(key);
        if (!node) throw std::out_of_range("ArtifactHashMap::at");
        return node->data.second;
    }
    
    const V& at(const K& key) const {
        const Node* node = findNode(key);
        if (!node) throw std::out_of_range("ArtifactHashMap::at");
        return node->data.second;
    }
    
    std::pair<iterator, bool> tryEmplace(const K& key, const V& value) {
        return emplaceImpl(key, value);
    }
    
    std::pair<iterator, bool> tryEmplace(const K& key, V&& value) {
        return emplaceImpl(key, std::move(value));
    }
    
    template<typename... Args>
    std::pair<iterator, bool> tryEmplace(const K& key, Args&&... args) {
        return emplaceImpl(key, V(std::forward<Args>(args)...));
    }
    
    template<typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        return emplaceImpl(std::forward<Args>(args)...);
    }
    
    size_type erase(const K& key) {
        const size_type hash = hasher_(key);
        const size_type bucketIdx = hash % bucketCount_;
        
        Node* prev = nullptr;
        Node* curr = buckets_[bucketIdx];
        
        while (curr) {
            if (curr->hash == hash && keyEqual_(curr->data.first, key)) {
                if (prev) {
                    prev->next = curr->next;
                } else {
                    buckets_[bucketIdx] = curr->next;
                }
                delete curr;
                --size_;
                return 1;
            }
            prev = curr;
            curr = curr->next;
        }
        return 0;
    }
    
    void clear() noexcept {
        for (size_type i = 0; i < bucketCount_; ++i) {
            Node* curr = buckets_[i];
            while (curr) {
                Node* next = curr->next;
                delete curr;
                curr = next;
            }
            buckets_[i] = nullptr;
        }
        size_ = 0;
    }
    
    iterator find(const K& key) noexcept {
        const size_type hash = hasher_(key);
        const size_type bucketIdx = hash % bucketCount_;
        
        Node* curr = buckets_[bucketIdx];
        while (curr) {
            if (curr->hash == hash && keyEqual_(curr->data.first, key)) {
                return iterator(curr, buckets_.get() + bucketIdx, buckets_.get() + bucketCount_);
            }
            curr = curr->next;
        }
        return end();
    }
    
    size_type count(const K& key) const noexcept {
        return findNode(key) ? 1 : 0;
    }
    
    bool contains(const K& key) const noexcept {
        return count(key) > 0;
    }
    
private:
    template<typename U>
    std::pair<iterator, bool> emplaceImpl(const K& key, U&& value) {
        const size_type hash = hasher_(key);
        const size_type bucketIdx = hash % bucketCount_;
        
        Node* curr = buckets_[bucketIdx];
        while (curr) {
            if (curr->hash == hash && keyEqual_(curr->data.first, key)) {
                curr->data.second = std::forward<U>(value);
                return {iterator(curr, buckets_.get() + bucketIdx, buckets_.get() + bucketCount_), false};
            }
            curr = curr->next;
        }
        
        if (static_cast<float>(size_ + 1) > static_cast<float>(bucketCount_) * kMaxLoadFactor) {
            rehash(bucketCount_ * 2);
            return emplaceImpl(key, std::forward<U>(value));
        }
        
        Node* newNode = new Node(hash, key, std::forward<U>(value));
        newNode->next = buckets_[bucketIdx];
        buckets_[bucketIdx] = newNode;
        ++size_;
        
        return {iterator(newNode, buckets_.get() + bucketIdx, buckets_.get() + bucketCount_), true};
    }
    
    Node* findNode(const K& key) const {
        const size_type hash = hasher_(key);
        const size_type bucketIdx = hash % bucketCount_;
        
        Node* curr = buckets_[bucketIdx];
        while (curr) {
            if (curr->hash == hash && keyEqual_(curr->data.first, key)) {
                return curr;
            }
            curr = curr->next;
        }
        return nullptr;
    }
    
    void rehash(size_type newBucketCount) {
        auto newBuckets = std::make_unique<Node*[]>(newBucketCount);
        for (size_type i = 0; i < newBucketCount; ++i) {
            newBuckets[i] = nullptr;
        }
        
        for (size_type i = 0; i < bucketCount_; ++i) {
            Node* curr = buckets_[i];
            while (curr) {
                Node* next = curr->next;
                const size_type newIdx = curr->hash % newBucketCount;
                curr->next = newBuckets[newIdx];
                newBuckets[newIdx] = curr;
                curr = next;
            }
        }
        
        buckets_ = std::move(newBuckets);
        bucketCount_ = newBucketCount;
    }
};

} // namespace ArtifactCore
