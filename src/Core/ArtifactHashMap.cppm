module;

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <functional>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <utility>

export module Core.ArtifactHashMap;

import Core.ArtifactOptional;

export namespace ArtifactCore {

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

        template<typename... Args>
        Node(size_type h, const K& key, std::piecewise_construct_t, Args&&... args)
            : data(std::piecewise_construct,
                   std::forward_as_tuple(key),
                   std::forward_as_tuple(std::forward<Args>(args)...)),
              hash(h), next(nullptr) {}
    };
    
    Node** buckets_ = nullptr;
    size_type bucketCount_;
    size_type size_;
    hasher hasher_;
    key_equal keyEqual_;
    
public:
    class const_iterator;

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
        
        iterator(const Node* node, Node* const* current, Node* const* end)
            : node_(const_cast<Node*>(node)), currentBucket_(const_cast<Node**>(current)), bucketsEnd_(const_cast<Node**>(end)) {}

        reference operator*() const noexcept { return node_->data; }
        pointer operator->() const noexcept { return &node_->data; }
        
        iterator& operator++() {
            if (node_->next != nullptr) {
                node_ = node_->next;
                return *this;
            }
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
        friend class const_iterator;
    };

    class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = typename ArtifactHashMap::value_type;
        using difference_type = typename ArtifactHashMap::difference_type;
        using pointer = const typename ArtifactHashMap::value_type*;
        using reference = const typename ArtifactHashMap::value_type&;

        const_iterator() noexcept : node_(nullptr), currentBucket_(nullptr), bucketsEnd_(nullptr) {}

        reference operator*() const noexcept { return node_->data; }
        pointer operator->() const noexcept { return &node_->data; }

        const_iterator& operator++() {
            if (node_->next != nullptr) {
                node_ = node_->next;
                return *this;
            }
            ++currentBucket_;
            advanceToNext();
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const const_iterator& other) const noexcept {
            return node_ == other.node_ && currentBucket_ == other.currentBucket_;
        }

        bool operator!=(const const_iterator& other) const noexcept {
            return !(*this == other);
        }

    private:
        const Node* node_;
        Node* const* currentBucket_;
        Node* const* bucketsEnd_;

        const_iterator(const Node* node, Node* const* current, Node* const* end)
            : node_(node), currentBucket_(current), bucketsEnd_(end) {}

        const_iterator(const iterator& other)
            : node_(other.node_), currentBucket_(other.currentBucket_), bucketsEnd_(other.bucketsEnd_) {}

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
        buckets_ = new Node*[bucketCount_];
        for (size_type i = 0; i < bucketCount_; ++i) {
            buckets_[i] = nullptr;
        }
    }
    
    ArtifactHashMap(size_type bucketCount)
        : bucketCount_(bucketCount == 0 ? 1 : bucketCount), size_(0) {
        buckets_ = new Node*[bucketCount_];
        for (size_type i = 0; i < bucketCount_; ++i) {
            buckets_[i] = nullptr;
        }
    }
    
    ~ArtifactHashMap() {
        clear();
        delete[] buckets_;
    }
    
    ArtifactHashMap(const ArtifactHashMap&) = delete;
    ArtifactHashMap& operator=(const ArtifactHashMap&) = delete;
    
    ArtifactHashMap(ArtifactHashMap&& other) noexcept
        : buckets_(other.buckets_),
          bucketCount_(other.bucketCount_), 
          size_(other.size_),
          hasher_(std::move(other.hasher_)),
          keyEqual_(std::move(other.keyEqual_)) {
        other.buckets_ = nullptr;
        other.bucketCount_ = 0;
        other.size_ = 0;
    }
    
    ArtifactHashMap& operator=(ArtifactHashMap&& other) noexcept {
        if (this == &other) return *this;
        clear();
        delete[] buckets_;
        buckets_ = other.buckets_;
        other.buckets_ = nullptr;
        bucketCount_ = other.bucketCount_;
        size_ = other.size_;
        hasher_ = std::move(other.hasher_);
        keyEqual_ = std::move(other.keyEqual_);
        other.bucketCount_ = 0;
        other.size_ = 0;
        return *this;
    }
    
    const_iterator find(const K& key) const noexcept {
        if (buckets_ == nullptr || bucketCount_ == 0) return end();
        const size_type hash = hasher_(key);
        const size_type bucketIdx = hash % bucketCount_;

        const Node* curr = buckets_[bucketIdx];
        while (curr) {
            if (curr->hash == hash && keyEqual_(curr->data.first, key)) {
                return const_iterator(curr, buckets_ + bucketIdx, buckets_ + bucketCount_);
            }
            curr = curr->next;
        }
        return end();
    }

    iterator begin() noexcept {
        if (buckets_ == nullptr || bucketCount_ == 0) return iterator();
        Node** bucket = buckets_;
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
    
    const_iterator begin() const noexcept {
        if (buckets_ == nullptr || bucketCount_ == 0) return const_iterator();
        Node** bucket = buckets_;
        Node** end = bucket + bucketCount_;

        Node** first = bucket;
        while (first != end && *first == nullptr) ++first;

        const_iterator it;
        if (first != end) {
            it = const_iterator(*first, first, end);
        } else {
            it = const_iterator(nullptr, end, end);
        }
        return it;
    }

    iterator end() noexcept {
        if (buckets_ == nullptr || bucketCount_ == 0) return iterator();
        return iterator(nullptr, buckets_ + bucketCount_, buckets_ + bucketCount_);
    }

    const_iterator end() const noexcept {
        if (buckets_ == nullptr || bucketCount_ == 0) return const_iterator();
        return const_iterator(nullptr, buckets_ + bucketCount_, buckets_ + bucketCount_);
    }
    
    size_type size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
    size_type bucket_count() const noexcept { return bucketCount_; }

    void reserve(size_type elementCount) {
        rehashStorage(minBucketCountFor((std::max)(elementCount, size_)));
    }

    void rehash(size_type requestedBuckets) {
        rehashStorage((std::max)(requestedBuckets, minBucketCountFor(size_)));
    }
    
    V& operator[](const K& key) {
        // NOTE: do not route through tryEmplace(key) here. The variadic
        // overload would resolve with an empty pack and overwrite an
        // existing entry with a default-constructed value on every read.
        if (Node* node = findNode(key)) {
            return node->data.second;
        }
        return tryEmplace(key, V()).first->second;
    }
    
    // Non-throwing lookup: returns Nullopt when the key is absent.
    Optional<V&> at(const K& key) {
        Node* node = findNode(key);
        if (!node) return Optional<V&>(Nullopt);
        return Optional<V&>(node->data.second);
    }

    Optional<const V&> at(const K& key) const {
        const Node* node = findNode(key);
        if (!node) return Optional<const V&>(Nullopt);
        return Optional<const V&>(node->data.second);
    }

    V* findValue(const K& key) {
        Node* node = findNode(key);
        return node ? &node->data.second : nullptr;
    }
    
    const V* findValue(const K& key) const {
        const Node* node = findNode(key);
        return node ? &node->data.second : nullptr;
    }
    
    std::pair<iterator, bool> tryEmplace(const K& key, const V& value) {
        return emplaceImpl(key, value);
    }
    
    std::pair<iterator, bool> tryEmplace(const K& key, V&& value) {
        return emplaceImpl(key, std::move(value));
    }
    
    template<typename... Args>
    std::pair<iterator, bool> tryEmplace(const K& key, Args&&... args) {
        return tryEmplaceImpl(key, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        return emplaceImpl(std::forward<Args>(args)...);
    }
    
    size_type erase(const K& key) {
        if (buckets_ == nullptr || bucketCount_ == 0) return 0;
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

    iterator erase(iterator position) {
        if (buckets_ == nullptr || bucketCount_ == 0 || position.node_ == nullptr
            || position.bucketsEnd_ != buckets_ + bucketCount_) {
            return end();
        }

        Node* const target = position.node_;
        Node** const bucket = position.currentBucket_;
        Node* previous = nullptr;
        Node* current = *bucket;
        while (current != nullptr && current != target) {
            previous = current;
            current = current->next;
        }
        if (current == nullptr) return end();

        Node* const nextInChain = current->next;
        if (previous != nullptr) previous->next = nextInChain;
        else *bucket = nextInChain;
        delete current;
        --size_;

        if (nextInChain != nullptr) {
            return iterator(nextInChain, bucket, buckets_ + bucketCount_);
        }

        Node** nextBucket = bucket + 1;
        Node** const bucketsEnd = buckets_ + bucketCount_;
        while (nextBucket != bucketsEnd && *nextBucket == nullptr) ++nextBucket;
        return nextBucket == bucketsEnd
            ? end()
            : iterator(*nextBucket, nextBucket, bucketsEnd);
    }
    
    void clear() noexcept {
        if (buckets_ == nullptr) {
            size_ = 0;
            return;
        }
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
        if (buckets_ == nullptr || bucketCount_ == 0) return end();
        const size_type hash = hasher_(key);
        const size_type bucketIdx = hash % bucketCount_;
        
        Node* curr = buckets_[bucketIdx];
        while (curr) {
            if (curr->hash == hash && keyEqual_(curr->data.first, key)) {
                return iterator(curr, buckets_ + bucketIdx, buckets_ + bucketCount_);
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
    static size_type minBucketCountFor(size_type elementCount) {
        const size_type quotient = elementCount / 3;
        const size_type remainder = elementCount % 3;
        const size_type maxSize = (std::numeric_limits<size_type>::max)();
        if (quotient > maxSize / 4) {
            throw std::length_error("ArtifactHashMap capacity is too large");
        }
        const size_type base = quotient * 4;
        const size_type extra = remainder == 0 ? 0 : remainder == 1 ? 2 : 3;
        if (base > maxSize - extra) {
            throw std::length_error("ArtifactHashMap capacity is too large");
        }
        const size_type requested = base + extra;
        return requested == 0 ? 1 : requested;
    }

    template<typename... Args>
    std::pair<iterator, bool> tryEmplaceImpl(const K& key, Args&&... args) {
        if (buckets_ == nullptr || bucketCount_ == 0) rehashStorage(kInitialBucketCount);
        const size_type hash = hasher_(key);
        size_type bucketIdx = hash % bucketCount_;

        for (Node* current = buckets_[bucketIdx]; current; current = current->next) {
            if (current->hash == hash && keyEqual_(current->data.first, key)) {
                return {iterator(current, buckets_ + bucketIdx, buckets_ + bucketCount_), false};
            }
        }

        if (static_cast<float>(size_ + 1) > static_cast<float>(bucketCount_) * kMaxLoadFactor) {
            rehashStorage(bucketCount_ * 2);
            bucketIdx = hash % bucketCount_;
        }

        Node* const newNode = new Node(hash, key, std::piecewise_construct,
                                      std::forward<Args>(args)...);
        newNode->next = buckets_[bucketIdx];
        buckets_[bucketIdx] = newNode;
        ++size_;
        return {iterator(newNode, buckets_ + bucketIdx, buckets_ + bucketCount_), true};
    }

    template<typename U>
    std::pair<iterator, bool> emplaceImpl(const K& key, U&& value) {
        if (buckets_ == nullptr || bucketCount_ == 0) rehashStorage(kInitialBucketCount);
        const size_type hash = hasher_(key);
        const size_type bucketIdx = hash % bucketCount_;
        
        Node* curr = buckets_[bucketIdx];
        while (curr) {
            if (curr->hash == hash && keyEqual_(curr->data.first, key)) {
                return {iterator(curr, buckets_ + bucketIdx, buckets_ + bucketCount_), false};
            }
            curr = curr->next;
        }
        
        if (static_cast<float>(size_ + 1) > static_cast<float>(bucketCount_) * kMaxLoadFactor) {
            rehashStorage(bucketCount_ * 2);
            return emplaceImpl(key, std::forward<U>(value));
        }
        
        Node* newNode = new Node(hash, key, std::forward<U>(value));
        newNode->next = buckets_[bucketIdx];
        buckets_[bucketIdx] = newNode;
        ++size_;
        
        return {iterator(newNode, buckets_ + bucketIdx, buckets_ + bucketCount_), true};
    }
    
    Node* findNode(const K& key) const {
        if (buckets_ == nullptr || bucketCount_ == 0) return nullptr;
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
    
    void rehashStorage(size_type newBucketCount) {
        if (newBucketCount == 0) newBucketCount = 1;
        Node** newBuckets = new Node*[newBucketCount];
        for (size_type i = 0; i < newBucketCount; ++i) {
            newBuckets[i] = nullptr;
        }
        
        for (size_type i = 0; buckets_ != nullptr && i < bucketCount_; ++i) {
            Node* curr = buckets_[i];
            while (curr) {
                Node* next = curr->next;
                const size_type newIdx = curr->hash % newBucketCount;
                curr->next = newBuckets[newIdx];
                newBuckets[newIdx] = curr;
                curr = next;
            }
        }
        
        delete[] buckets_;
        buckets_ = newBuckets;
        bucketCount_ = newBucketCount;
    }
};

} // namespace ArtifactCore
