module;
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <new>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

export module Container.SmallVector;

import Container.Debug;
import Container.NamedVector;

export namespace ArtifactCore {

template <typename T, std::size_t InlineCapacity = 4>
class SmallVector {
public:
  using Value = T;
  static constexpr std::size_t kInlineCapacity = InlineCapacity;

  SmallVector() noexcept = default;

  explicit SmallVector(ContainerName name) noexcept
    : name_(name)
  {
  }

  SmallVector(ContainerName name, ContainerSourceLocation createdAt) noexcept
    : name_(name)
    , createdAt_(createdAt)
  {
  }

  SmallVector(ContainerName name, ContainerDomain domain) noexcept
    : name_(name)
    , domain_(domain)
  {
  }

  SmallVector(ContainerName name, ContainerDomain domain, ContainerSourceLocation createdAt) noexcept
    : name_(name)
    , domain_(domain)
    , createdAt_(createdAt)
  {
  }

  SmallVector(ContainerName name, std::initializer_list<T> values)
    : name_(name)
  {
    reserve(values.size());
    for (const auto& v : values) {
      add(v);
    }
  }

  ~SmallVector()
  {
    destroyRange(0, size_);
    if (!isInline()) {
      ::operator delete(heap_);
    }
  }

  SmallVector(const SmallVector& other)
    : name_(other.name_)
    , domain_(other.domain_)
    , owner_(other.owner_)
    , createdAt_(other.createdAt_)
    , counters_(other.counters_)
    , lastMutatedAt_(other.lastMutatedAt_)
    , lastFailedAccessAt_(other.lastFailedAccessAt_)
    , lastMutation_(other.lastMutation_)
  {
    mutationHistory_ = other.mutationHistory_;
    reserve(other.size_);
    for (std::size_t i = 0; i < other.size_; ++i) {
      new (data_ + i) T(other.data_[i]);
    }
    size_ = other.size_;
  }

  SmallVector& operator=(const SmallVector& other)
  {
    if (this != &other) {
      destroyRange(0, size_);
      reserve(other.size_);
      for (std::size_t i = 0; i < other.size_; ++i) {
        new (data_ + i) T(other.data_[i]);
      }
      size_ = other.size_;
      name_ = other.name_;
      domain_ = other.domain_;
      owner_ = other.owner_;
      createdAt_ = other.createdAt_;
      counters_ = other.counters_;
      lastMutatedAt_ = other.lastMutatedAt_;
      lastFailedAccessAt_ = other.lastFailedAccessAt_;
      lastMutation_ = other.lastMutation_;
      mutationHistory_ = other.mutationHistory_;
    }
    return *this;
  }

  SmallVector(SmallVector&& other) noexcept
    : name_(other.name_)
    , domain_(other.domain_)
    , owner_(other.owner_)
    , createdAt_(other.createdAt_)
    , counters_(other.counters_)
    , lastMutatedAt_(other.lastMutatedAt_)
    , lastFailedAccessAt_(other.lastFailedAccessAt_)
    , lastMutation_(other.lastMutation_)
  {
    mutationHistory_ = std::move(other.mutationHistory_);
    if (!other.isInline()) {
      data_ = other.data_;
      heap_ = other.heap_;
      capacity_ = other.capacity_;
    } else {
      for (std::size_t i = 0; i < other.size_; ++i) {
        new (data_ + i) T(std::move(other.data_[i]));
        other.data_[i].~T();
      }
    }
    size_ = other.size_;
    other.data_ = reinterpret_cast<T*>(other.inline_);
    other.size_ = 0;
    other.capacity_ = kInlineCapacity;
    other.heap_ = nullptr;
  }

  SmallVector& operator=(SmallVector&& other) noexcept
  {
    if (this != &other) {
      destroyRange(0, size_);
      if (!isInline()) {
        ::operator delete(heap_);
        heap_ = nullptr;
      }
      data_ = reinterpret_cast<T*>(inline_);
      capacity_ = kInlineCapacity;

      if (!other.isInline()) {
        data_ = other.data_;
        heap_ = other.heap_;
        capacity_ = other.capacity_;
      } else {
        for (std::size_t i = 0; i < other.size_; ++i) {
          new (data_ + i) T(std::move(other.data_[i]));
          other.data_[i].~T();
        }
      }
      size_ = other.size_;
      name_ = other.name_;
      domain_ = other.domain_;
      owner_ = other.owner_;
      createdAt_ = other.createdAt_;
      counters_ = other.counters_;
      lastMutatedAt_ = other.lastMutatedAt_;
      lastFailedAccessAt_ = other.lastFailedAccessAt_;
      lastMutation_ = other.lastMutation_;
      mutationHistory_ = std::move(other.mutationHistory_);

      other.data_ = reinterpret_cast<T*>(other.inline_);
      other.size_ = 0;
      other.capacity_ = kInlineCapacity;
      other.heap_ = nullptr;
    }
    return *this;
  }

  std::size_t count() const noexcept
  {
    ++counters_.readCount;
    return size_;
  }

  std::size_t size() const noexcept
  {
    return count();
  }

  bool isEmpty() const noexcept
  {
    ++counters_.readCount;
    return size_ == 0;
  }

  bool notEmpty() const noexcept
  {
    return !isEmpty();
  }

  std::size_t capacity() const noexcept
  {
    ++counters_.readCount;
    return capacity_;
  }

  void reserve(std::size_t newCapacity)
  {
    if (newCapacity <= capacity_) {
      return;
    }
    grow(newCapacity);
  }

  void resize(std::size_t newSize)
  {
    const auto before = size_;
    if (newSize > size_) {
      reserve(newSize);
      for (std::size_t i = size_; i < newSize; ++i) {
        new (data_ + i) T();
      }
    } else if (newSize < size_) {
      destroyRange(newSize, size_);
    }
    size_ = newSize;
    updateMaxCount();
    recordMutation("resize", before, size_);
  }

  void clear() noexcept
  {
    const auto before = size_;
    destroyRange(0, size_);
    size_ = 0;
    recordMutation("clear", before, 0);
  }

  void add(const T& value)
  {
    const auto before = size_;
    maybeGrow();
    new (data_ + size_) T(value);
    ++size_;
    updateMaxCount();
    recordMutation("add", before, size_);
  }

  void append(const T& value)
  {
    add(value);
  }

  void add(T&& value)
  {
    const auto before = size_;
    maybeGrow();
    new (data_ + size_) T(std::move(value));
    ++size_;
    updateMaxCount();
    recordMutation("add", before, size_);
  }

  void append(T&& value)
  {
    add(std::move(value));
  }

  void insert(std::size_t index, const T& value)
  {
    if (index >= size_) {
      add(value);
      return;
    }
    const auto before = size_;
    maybeGrow();
    shiftRight(index, 1);
    data_[index] = value;
    ++size_;
    updateMaxCount();
    recordMutation("insert", before, size_);
  }

  void insert(std::size_t index, T&& value)
  {
    if (index >= size_) {
      add(std::move(value));
      return;
    }
    const auto before = size_;
    maybeGrow();
    shiftRight(index, 1);
    data_[index] = std::move(value);
    ++size_;
    updateMaxCount();
    recordMutation("insert", before, size_);
  }

  void assign(std::initializer_list<T> values)
  {
    const auto before = size_;
    clear();
    reserve(values.size());
    for (const auto& v : values) {
      new (data_ + size_) T(v);
      ++size_;
    }
    updateMaxCount();
    recordMutation("assign", before, size_);
  }

  template <typename... Args>
  T& make(Args&&... args)
  {
    const auto before = size_;
    maybeGrow();
    T* value = new (data_ + size_) T(std::forward<Args>(args)...);
    ++size_;
    updateMaxCount();
    recordMutation("make", before, size_);
    return *value;
  }

  bool hasIndex(std::size_t index) const noexcept
  {
    ++counters_.readCount;
    return index < size_;
  }

  T* at(std::size_t index) noexcept
  {
    if (!hasIndex(index)) {
      bumpFailedAccess();
      return nullptr;
    }
    return &data_[index];
  }

  const T* at(std::size_t index) const noexcept
  {
    if (!hasIndex(index)) {
      bumpFailedAccess();
      return nullptr;
    }
    return &data_[index];
  }

  T& operator[](std::size_t index) noexcept
  {
    return data_[index];
  }

  const T& operator[](std::size_t index) const noexcept
  {
    return data_[index];
  }

  T* first() noexcept
  {
    if (isEmpty()) {
      bumpFailedAccess();
      return nullptr;
    }
    ++counters_.readCount;
    return data_;
  }

  const T* first() const noexcept
  {
    if (isEmpty()) {
      bumpFailedAccess();
      return nullptr;
    }
    ++counters_.readCount;
    return data_;
  }

  T* last() noexcept
  {
    if (isEmpty()) {
      bumpFailedAccess();
      return nullptr;
    }
    ++counters_.readCount;
    return data_ + size_ - 1;
  }

  const T* last() const noexcept
  {
    if (isEmpty()) {
      bumpFailedAccess();
      return nullptr;
    }
    ++counters_.readCount;
    return data_ + size_ - 1;
  }

  T* front() noexcept
  {
    return first();
  }

  const T* front() const noexcept
  {
    return first();
  }

  T* back() noexcept
  {
    return last();
  }

  const T* back() const noexcept
  {
    return last();
  }

  bool removeAt(std::size_t index)
  {
    if (!hasIndex(index)) {
      bumpFailedAccess();
      return false;
    }
    const auto before = size_;
    data_[index].~T();
    shiftLeft(index + 1, 1);
    --size_;
    recordMutation("removeAt", before, size_);
    return true;
  }

  bool replace(std::size_t index, const T& value)
  {
    if (!hasIndex(index)) {
      bumpFailedAccess();
      return false;
    }
    data_[index] = value;
    recordMutation("replace", size_, size_);
    return true;
  }

  bool swapItemsAt(std::size_t firstIndex, std::size_t secondIndex)
  {
    if (!hasIndex(firstIndex) || !hasIndex(secondIndex)) {
      bumpFailedAccess();
      return false;
    }
    if (firstIndex != secondIndex) {
      std::swap(data_[firstIndex], data_[secondIndex]);
      recordMutation("swapItemsAt", size_, size_);
    }
    return true;
  }

  bool popBack() noexcept
  {
    if (isEmpty()) {
      bumpFailedAccess();
      return false;
    }
    const auto before = size_;
    data_[size_ - 1].~T();
    --size_;
    recordMutation("popBack", before, size_);
    return true;
  }

  bool popFront() noexcept
  {
    return removeAt(0);
  }

  bool removeFirst() noexcept
  {
    return popFront();
  }

  T takeAt(std::size_t index)
  {
    if (!hasIndex(index)) {
      bumpFailedAccess();
      return T{};
    }
    const auto before = size_;
    T value = std::move(data_[index]);
    data_[index].~T();
    shiftLeft(index + 1, 1);
    --size_;
    recordMutation("takeAt", before, size_);
    return value;
  }

  T takeFirst()
  {
    if (isEmpty()) {
      bumpFailedAccess();
      return T{};
    }
    return takeAt(0);
  }

  T takeLast()
  {
    if (isEmpty()) {
      bumpFailedAccess();
      return T{};
    }
    const auto before = size_;
    T value = std::move(data_[size_ - 1]);
    data_[size_ - 1].~T();
    --size_;
    recordMutation("takeLast", before, size_);
    return value;
  }

  T* begin() noexcept
  {
    return data_;
  }

  T* end() noexcept
  {
    return data_ + size_;
  }

  const T* begin() const noexcept
  {
    return data_;
  }

  const T* end() const noexcept
  {
    return data_ + size_;
  }

  bool contains(const T& value) const
  {
    for (std::size_t i = 0; i < size_; ++i) {
      if (data_[i] == value) {
        return true;
      }
    }
    return false;
  }

  std::ptrdiff_t indexOf(const T& value, std::size_t from = 0) const
  {
    ++counters_.readCount;
    for (std::size_t index = from; index < size_; ++index) {
      if (data_[index] == value) return static_cast<std::ptrdiff_t>(index);
    }
    return -1;
  }

  std::ptrdiff_t lastIndexOf(const T& value) const
  {
    ++counters_.readCount;
    for (std::size_t index = size_; index > 0; --index) {
      if (data_[index - 1] == value) return static_cast<std::ptrdiff_t>(index - 1);
    }
    return -1;
  }

  bool startsWith(const T& value) const
  {
    ++counters_.readCount;
    return size_ != 0 && data_[0] == value;
  }

  bool endsWith(const T& value) const
  {
    ++counters_.readCount;
    return size_ != 0 && data_[size_ - 1] == value;
  }

  bool removeOne(const T& value)
  {
    const auto index = indexOf(value);
    return index >= 0 && removeAt(static_cast<std::size_t>(index));
  }

  std::size_t removeAll(const T& value)
  {
    return removeIf([&value](const T& item) { return item == value; });
  }

  template <typename Predicate>
  std::size_t removeIf(Predicate&& predicate)
  {
    const auto before = size_;
    std::size_t writeIndex = 0;
    for (std::size_t readIndex = 0; readIndex < size_; ++readIndex) {
      if (!predicate(data_[readIndex])) {
        if (writeIndex != readIndex) data_[writeIndex] = std::move(data_[readIndex]);
        ++writeIndex;
      }
    }
    destroyRange(writeIndex, size_);
    size_ = writeIndex;
    const auto removed = before - size_;
    if (removed != 0) recordMutation("removeIf", before, size_);
    return removed;
  }

  T* data() noexcept { return data_; }
  const T* data() const noexcept { return data_; }
  const T* constData() const noexcept { return data_; }

  std::vector<T> toStdVector() const
  {
    std::vector<T> result;
    result.reserve(size_);
    for (std::size_t i = 0; i < size_; ++i) {
      result.push_back(data_[i]);
    }
    return result;
  }

  template <typename F>
  void each(F&& fn) const
  {
    for (std::size_t i = 0; i < size_; ++i) {
      fn(data_[i]);
    }
  }

  template <typename F>
  void eachIndexed(F&& fn) const
  {
    for (std::size_t i = 0; i < size_; ++i) {
      fn(i, data_[i]);
    }
  }

  template <typename F>
  void mutableEach(F&& fn)
  {
    for (std::size_t i = 0; i < size_; ++i) {
      fn(data_[i]);
    }
  }

  ContainerName name() const noexcept
  {
    return name_;
  }

  void setName(ContainerName name) noexcept
  {
    name_ = name;
  }

  ContainerDomain domain() const noexcept
  {
    return domain_;
  }

  void setDomain(ContainerDomain domain) noexcept
  {
    domain_ = domain;
  }

  ContainerOwner owner() const noexcept
  {
    return owner_;
  }

  void setOwner(ContainerOwner owner) noexcept
  {
    owner_ = owner;
  }

  void setCreatedAt(ContainerSourceLocation location) noexcept
  {
    createdAt_ = location;
  }

  void markCreatedHere(ContainerSourceLocation location) noexcept
  {
    createdAt_ = location;
  }

  void setLastMutatedAt(ContainerSourceLocation location) noexcept
  {
    lastMutatedAt_ = location;
  }

  void setLastFailedAccessAt(ContainerSourceLocation location) noexcept
  {
    lastFailedAccessAt_ = location;
  }

  const ContainerDebugCounters& counters() const noexcept
  {
    return counters_;
  }

  ContainerDebugCheckpoint debugCheckpoint() const noexcept
  {
    return ContainerDebugCheckpoint{counters_.version, counters_.readCount, counters_.failedAccessCount};
  }

  template <typename Callback>
  bool watchSince(const ContainerDebugCheckpoint& checkpoint, Callback&& callback) const
  {
    const bool hit = counters_.version != checkpoint.version
      || counters_.failedAccessCount != checkpoint.failedAccessCount;
    if (hit) callback(ContainerWatchHit{"smallvector-changed-since-checkpoint", createdAt_, debugSnapshot()});
    return hit;
  }

  const ContainerMutationRecord& lastMutation() const noexcept
  {
    return lastMutation_;
  }

  std::size_t mutationHistorySize() const noexcept
  {
    return mutationHistory_.size();
  }

  const ContainerMutationRecord* mutationHistoryAt(std::size_t index) const noexcept
  {
    return index < mutationHistory_.size() ? &mutationHistory_[index] : nullptr;
  }

  ContainerDebugInfo debugInfo() const noexcept
  {
    return ContainerDebugInfo{
      name_,
      domain_,
      owner_,
      typeid(T).name(),
      size_,
      capacity_,
      capacity_ * sizeof(T)
    };
  }

  ContainerDebugSnapshot debugSnapshot() const noexcept
  {
    const auto samples = debugSample(4);
    return ContainerDebugSnapshot{
      debugInfo(),
      counters_,
      createdAt_,
      lastMutatedAt_,
      lastFailedAccessAt_,
      lastMutation_,
      samples
    };
  }

  NamedVector<ContainerElementSample> debugSample(std::size_t limit = 4) const
  {
    NamedVector<ContainerElementSample> samples{ContainerName{"SmallVectorElementSamples"}};
    const auto sampleCount = size_ < limit ? size_ : limit;
    for (std::size_t i = 0; i < sampleCount; ++i) {
      samples.add(ContainerElementSample{i, static_cast<const void*>(&data_[i]), "sample"});
    }
    return samples;
  }

  template <typename Callback>
  bool watch(const ContainerWatchRule& rule, Callback&& callback) const
  {
    const auto snapshot = debugSnapshot();
    const auto cnt = snapshot.info.count;
    const auto ver = snapshot.counters.version;
    const auto readCnt = snapshot.counters.readCount;
    const auto failedAccess = snapshot.counters.failedAccessCount > 0;
    const auto mutated = snapshot.counters.mutationCount > 0;
    const auto empty = snapshot.info.count == 0;
    const bool hit = (rule.minCount != 0 && cnt < rule.minCount)
      || (rule.maxCount != 0 && cnt > rule.maxCount)
      || (rule.minVersion != 0 && ver < rule.minVersion)
      || (rule.maxVersion != 0 && ver > rule.maxVersion)
      || (rule.minReadCount != 0 && readCnt < rule.minReadCount)
      || (rule.maxReadCount != 0 && readCnt > rule.maxReadCount)
      || (rule.watchEmpty && empty)
      || (rule.watchFailedAccess && failedAccess)
      || (rule.watchMutation && mutated);
    if (hit) {
      callback(ContainerWatchHit{"smallvector-watch", createdAt_, snapshot});
    }
    return hit;
  }

private:
  bool isInline() const noexcept
  {
    return heap_ == nullptr;
  }

  void maybeGrow()
  {
    if (size_ >= capacity_) {
      grow(capacity_ * 2 + 1);
    }
  }

  void grow(std::size_t newCapacity)
  {
    T* newData = static_cast<T*>(::operator new(sizeof(T) * newCapacity));
    for (std::size_t i = 0; i < size_; ++i) {
      new (newData + i) T(std::move(data_[i]));
      data_[i].~T();
    }
    if (!isInline()) {
      ::operator delete(heap_);
    }
    data_ = newData;
    heap_ = newData;
    capacity_ = newCapacity;
  }

  void shiftRight(std::size_t start, std::size_t count)
  {
    new (data_ + size_) T(std::move(data_[size_ - 1]));
    for (std::size_t i = size_ - 1; i > start; --i) {
      data_[i] = std::move(data_[i - 1]);
    }
  }

  void shiftLeft(std::size_t start, std::size_t count)
  {
    for (std::size_t i = start; i < size_; ++i) {
      data_[i - count] = std::move(data_[i]);
    }
  }

  void destroyRange(std::size_t begin, std::size_t end) noexcept
  {
    for (std::size_t i = begin; i < end; ++i) {
      data_[i].~T();
    }
  }

  void updateMaxCount() noexcept
  {
    if (size_ > counters_.maxCountSeen) {
      counters_.maxCountSeen = size_;
    }
  }

  void recordMutation(const char* operation, std::size_t before, std::size_t after) noexcept
  {
    ++counters_.version;
    ++counters_.mutationCount;
    if (after > counters_.maxCountSeen) counters_.maxCountSeen = after;
    if (after > before) counters_.addedCount += after - before;
    if (before > after) counters_.removedCount += before - after;
    if (capacity_ != observedCapacity_) {
      ++counters_.capacityChangeCount;
      observedCapacity_ = capacity_;
    }
    if (capacity_ > counters_.maxCapacitySeen) counters_.maxCapacitySeen = capacity_;
    const auto approximateBytes = capacity_ * sizeof(T);
    if (approximateBytes > counters_.maxApproximateBytesSeen) counters_.maxApproximateBytesSeen = approximateBytes;
    lastMutatedAt_ = createdAt_;
    lastMutation_ = ContainerMutationRecord{
      operation,
      createdAt_,
      counters_.version,
      before,
      after,
      ""
    };
    pushMutation(lastMutation_);
  }

  void bumpFailedAccess() const noexcept
  {
    ++counters_.failedAccessCount;
    lastFailedAccessAt_ = createdAt_;
  }

  void pushMutation(const ContainerMutationRecord& record) noexcept
  {
    constexpr std::size_t historyCapacity = 8;
    if (mutationHistory_.size() == historyCapacity) {
      mutationHistory_.erase(mutationHistory_.begin());
    }
    mutationHistory_.push_back(record);
  }

  alignas(T) unsigned char inline_[sizeof(T) * kInlineCapacity]{};
  T* data_ = reinterpret_cast<T*>(inline_);
  std::size_t size_ = 0;
  std::size_t capacity_ = kInlineCapacity;
  T* heap_ = nullptr;

  ContainerName name_;
  ContainerDomain domain_ = ContainerDomain::Unknown;
  ContainerOwner owner_{};
  mutable ContainerDebugCounters counters_{};
  ContainerSourceLocation createdAt_{};
  ContainerSourceLocation lastMutatedAt_{};
  mutable ContainerSourceLocation lastFailedAccessAt_{};
  ContainerMutationRecord lastMutation_{};
  std::vector<ContainerMutationRecord> mutationHistory_;
  std::size_t observedCapacity_ = kInlineCapacity;
};

template <typename T, std::size_t N = 4>
SmallVector<T, N> makeSmallVector(ContainerName name)
{
  return SmallVector<T, N>(name);
}

template <typename T, std::size_t N = 4>
SmallVector<T, N> makeSmallVector(ContainerName name, ContainerSourceLocation createdAt)
{
  return SmallVector<T, N>(name, createdAt);
}

template <typename T, std::size_t N = 4>
SmallVector<T, N> makeSmallVector(const char* name)
{
  return SmallVector<T, N>(ContainerName{name});
}

template <typename T, std::size_t N = 4>
SmallVector<T, N> makeSmallVector(const char* name, ContainerSourceLocation createdAt)
{
  return SmallVector<T, N>(ContainerName{name}, createdAt);
}

}
