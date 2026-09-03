module;
#include <cstddef>
#include <initializer_list>
#include <functional>
#include <memory>
#include <algorithm>
#include <typeinfo>
#include <utility>
#include <string>
#include <vector>

export module Container.NamedVector;

import Container.Debug;
import Container.Debug.Registry;

export namespace ArtifactCore {

template <typename T>
class NamedVector {
public:
  using Value = T;

  NamedVector() = default;

  explicit NamedVector(ContainerName name) noexcept
    : name_(name)
  {
  }

  NamedVector(ContainerName name, ContainerSourceLocation createdAt) noexcept
    : name_(name)
    , createdAt_(createdAt)
  {
  }

  NamedVector(ContainerName name, ContainerDomain domain) noexcept
    : name_(name)
    , domain_(domain)
  {
  }

  NamedVector(ContainerName name, ContainerDomain domain, ContainerSourceLocation createdAt) noexcept
    : name_(name)
    , domain_(domain)
    , createdAt_(createdAt)
  {
  }

  NamedVector(ContainerName name, std::initializer_list<T> values)
    : values_(values)
    , name_(name)
  {
    counters_.maxCountSeen = values_.size();
  }

  std::size_t count() const noexcept
  {
    ++counters_.readCount;
    return values_.size();
  }

  std::size_t size() const noexcept
  {
    return count();
  }

  bool isEmpty() const noexcept
  {
    ++counters_.readCount;
    return values_.empty();
  }

  bool notEmpty() const noexcept
  {
    return !isEmpty();
  }

  // STL-compatible aliases retained for existing module clients.  The
  // named-container operations above remain the preferred API, while these
  // aliases keep older algorithm-heavy code source-compatible.
  bool empty() const noexcept
  {
    return isEmpty();
  }

  std::size_t capacity() const noexcept
  {
    ++counters_.readCount;
    return values_.capacity();
  }

  void reserve(std::size_t capacity)
  {
    const auto before = values_.size();
    values_.reserve(capacity);
    recordMutation("reserve", before, values_.size());
  }

  void squeeze()
  {
    const auto before = values_.size();
    values_.shrink_to_fit();
    recordMutation("squeeze", before, values_.size());
  }

  void resize(std::size_t newSize)
  {
    const auto before = values_.size();
    values_.resize(newSize);
    recordMutation("resize", before, values_.size());
  }

  void clear() noexcept
  {
    const auto before = values_.size();
    values_.clear();
    recordMutation("clear", before, values_.size());
  }

  void add(const T& value)
  {
    const auto before = values_.size();
    values_.push_back(value);
    recordMutation("add", before, values_.size());
  }

  void append(const T& value)
  {
    add(value);
  }

  void add(T&& value)
  {
    const auto before = values_.size();
    values_.push_back(std::move(value));
    recordMutation("add", before, values_.size());
  }

  void push_back(const T& value)
  {
    add(value);
  }

  void push_back(T&& value)
  {
    add(std::move(value));
  }

  void append(T&& value)
  {
    add(std::move(value));
  }

  void insert(std::size_t index, const T& value)
  {
    if (index >= values_.size()) {
      add(value);
      return;
    }
    const auto before = values_.size();
    values_.insert(values_.begin() + static_cast<typename std::vector<T>::difference_type>(index), value);
    recordMutation("insert", before, values_.size());
  }

  void insert(std::size_t index, T&& value)
  {
    if (index >= values_.size()) {
      add(std::move(value));
      return;
    }
    const auto before = values_.size();
    values_.insert(values_.begin() + static_cast<typename std::vector<T>::difference_type>(index), std::move(value));
    recordMutation("insert", before, values_.size());
  }

  void assign(std::initializer_list<T> values)
  {
    const auto before = values_.size();
    values_.assign(values.begin(), values.end());
    recordMutation("assign", before, values_.size());
  }

  template <typename... Args>
  T& make(Args&&... args)
  {
    const auto before = values_.size();
    auto& value = values_.emplace_back(std::forward<Args>(args)...);
    recordMutation("make", before, values_.size());
    return value;
  }

  template <typename... Args>
  T& emplace_back(Args&&... args)
  {
    return make(std::forward<Args>(args)...);
  }

  bool hasIndex(std::size_t index) const noexcept
  {
    ++counters_.readCount;
    return index < values_.size();
  }

  T* at(std::size_t index) noexcept
  {
    if (!hasIndex(index)) {
      bumpFailedAccess();
      return nullptr;
    }
    return &values_[index];
  }

  T& operator[](std::size_t index) noexcept
  {
    return *at(index);
  }

  const T& operator[](std::size_t index) const noexcept
  {
    return *at(index);
  }

  const T* at(std::size_t index) const noexcept
  {
    if (!hasIndex(index)) {
      bumpFailedAccess();
      return nullptr;
    }
    return &values_[index];
  }

  T* first() noexcept
  {
    if (values_.empty()) {
      bumpFailedAccess();
      return nullptr;
    }
    ++counters_.readCount;
    return &values_.front();
  }

  const T* first() const noexcept
  {
    if (values_.empty()) {
      bumpFailedAccess();
      return nullptr;
    }
    ++counters_.readCount;
    return &values_.front();
  }

  T* last() noexcept
  {
    if (values_.empty()) {
      bumpFailedAccess();
      return nullptr;
    }
    ++counters_.readCount;
    return &values_.back();
  }

  const T* last() const noexcept
  {
    if (values_.empty()) {
      bumpFailedAccess();
      return nullptr;
    }
    ++counters_.readCount;
    return &values_.back();
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
    const auto before = values_.size();
    values_.erase(values_.begin() + static_cast<typename std::vector<T>::difference_type>(index));
    recordMutation("removeAt", before, values_.size());
    return true;
  }

  bool replace(std::size_t index, const T& value)
  {
    if (!hasIndex(index)) {
      bumpFailedAccess();
      return false;
    }
    values_[index] = value;
    recordMutation("replace", values_.size(), values_.size());
    return true;
  }

  bool move(std::size_t from, std::size_t to)
  {
    if (!hasIndex(from) || !hasIndex(to)) {
      bumpFailedAccess();
      return false;
    }
    if (from == to) return true;
    auto value = std::move(values_[from]);
    values_.erase(values_.begin() + static_cast<typename std::vector<T>::difference_type>(from));
    values_.insert(values_.begin() + static_cast<typename std::vector<T>::difference_type>(to), std::move(value));
    recordMutation("move", values_.size(), values_.size());
    return true;
  }

  bool swapItemsAt(std::size_t firstIndex, std::size_t secondIndex)
  {
    if (!hasIndex(firstIndex) || !hasIndex(secondIndex)) {
      bumpFailedAccess();
      return false;
    }
    if (firstIndex != secondIndex) {
      std::swap(values_[firstIndex], values_[secondIndex]);
      recordMutation("swapItemsAt", values_.size(), values_.size());
    }
    return true;
  }

  bool popBack() noexcept
  {
    if (values_.empty()) {
      bumpFailedAccess();
      return false;
    }
    const auto before = values_.size();
    values_.pop_back();
    recordMutation("popBack", before, values_.size());
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
    const auto before = values_.size();
    T value = std::move(values_[index]);
    values_.erase(values_.begin() + static_cast<typename std::vector<T>::difference_type>(index));
    recordMutation("takeAt", before, values_.size());
    return value;
  }

  bool tryTakeAt(std::size_t index, T& result)
  {
    if (!hasIndex(index)) {
      bumpFailedAccess();
      return false;
    }
    const auto before = values_.size();
    result = std::move(values_[index]);
    values_.erase(values_.begin() + static_cast<typename std::vector<T>::difference_type>(index));
    recordMutation("tryTakeAt", before, values_.size());
    return true;
  }

  T takeFirst()
  {
    if (values_.empty()) {
      bumpFailedAccess();
      return T{};
    }
    return takeAt(0);
  }

  T takeLast()
  {
    if (values_.empty()) {
      bumpFailedAccess();
      return T{};
    }
    return takeAt(values_.size() - 1);
  }

  bool contains(const T& value) const
  {
    for (const auto& item : values_) {
      if (item == value) {
        return true;
      }
    }
    return false;
  }

  std::ptrdiff_t indexOf(const T& value, std::size_t from = 0) const
  {
    ++counters_.readCount;
    for (std::size_t index = from; index < values_.size(); ++index) {
      if (values_[index] == value) return static_cast<std::ptrdiff_t>(index);
    }
    return -1;
  }

  std::ptrdiff_t lastIndexOf(const T& value) const
  {
    ++counters_.readCount;
    for (std::size_t index = values_.size(); index > 0; --index) {
      if (values_[index - 1] == value) return static_cast<std::ptrdiff_t>(index - 1);
    }
    return -1;
  }

  bool startsWith(const T& value) const
  {
    ++counters_.readCount;
    return !values_.empty() && values_.front() == value;
  }

  bool endsWith(const T& value) const
  {
    ++counters_.readCount;
    return !values_.empty() && values_.back() == value;
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
    const auto before = values_.size();
    values_.erase(std::remove_if(values_.begin(), values_.end(), std::forward<Predicate>(predicate)), values_.end());
    const auto removed = before - values_.size();
    if (removed != 0) recordMutation("removeIf", before, values_.size());
    return removed;
  }

  T* data() noexcept { return values_.data(); }
  const T* data() const noexcept { return values_.data(); }
  const T* constData() const noexcept { return values_.data(); }

  std::vector<T> toStdVector() const
  {
    return values_;
  }

  static NamedVector fromStdVector(ContainerName name, std::vector<T> values)
  {
    NamedVector out(name);
    out.values_ = std::move(values);
    out.counters_.maxCountSeen = out.values_.size();
    return out;
  }

  template <typename F>
  void each(F&& fn) const
  {
    for (const auto& value : values_) {
      fn(value);
    }
  }

  template <typename F>
  void eachIndexed(F&& fn) const
  {
    std::size_t index = 0;
    for (const auto& value : values_) {
      fn(index, value);
      ++index;
    }
  }

  template <typename F>
  void mutableEach(F&& fn)
  {
    for (auto& value : values_) {
      fn(value);
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

  ContainerDebugInfo debugInfo() const noexcept
  {
    return ContainerDebugInfo{
      name_,
      domain_,
      owner_,
      typeid(T).name(),
      values_.size(),
      values_.capacity(),
      values_.capacity() * sizeof(T)
    };
  }

  ContainerDebugSnapshot debugSnapshot() const noexcept
  {
    const auto samples = debugSample(4);
    auto snapshot = ContainerDebugSnapshot{
      debugInfo(),
      counters_,
      createdAt_,
      lastMutatedAt_,
      lastFailedAccessAt_,
      lastMutation_,
      samples
    };
    snapshot.notes = debugNotes_;
    return snapshot;
  }

  bool addDebugNote(std::string text,
                    ContainerDebugNoteSeverity severity = ContainerDebugNoteSeverity::Info,
                    ContainerDebugNoteAuthor author = ContainerDebugNoteAuthor::Runtime,
                    ContainerSourceLocation location = {})
  {
    constexpr std::size_t maxNoteBytes = 1024;
    if (text.empty() || text.size() > maxNoteBytes ||
        !isValidContainerDebugNoteSeverity(severity) ||
        !isValidContainerDebugNoteAuthor(author)) {
      return false;
    }
    constexpr std::size_t noteCapacity = 32;
    if (debugNotes_.size() == noteCapacity) debugNotes_.erase(debugNotes_.begin());
    debugNotes_.push_back(ContainerDebugNote{
      containerDebugNowMilliseconds(), severity, author, std::move(text), location, counters_.version});
    return true;
  }

  const std::vector<ContainerDebugNote>& debugNotes() const noexcept { return debugNotes_; }

  ContainerDebugRegistry::Registration registerDebugSnapshot(
    ContainerDebugRegistry& registry, std::string id)
  {
    return registry.registerScoped(
      std::move(id),
      [this]() { return debugSnapshot(); },
      [this](std::string text, ContainerDebugNoteSeverity severity, ContainerDebugNoteAuthor author) {
        return addDebugNote(std::move(text), severity, author);
      });
  }

  ContainerDebugRegistry::Registration registerDebugSnapshot(std::string id)
  {
    return registerDebugSnapshot(ContainerDebugRegistry::instance(), std::move(id));
  }

  ContainerDebugValueCheckpoint<T> createDebugCheckpoint() const
  {
    return ContainerDebugValueCheckpoint<T>{
      this, containerDebugNowMilliseconds(), counters_.version, counters_.failedAccessCount, values_};
  }

  ContainerDebugVerification verifyDebugCheckpoint(
    const ContainerDebugValueCheckpoint<T>& checkpoint) const noexcept
  {
    const bool sourceMatches = checkpoint.source == this;
    return ContainerDebugVerification{
      sourceMatches,
      sourceMatches && checkpoint.version == counters_.version
        && checkpoint.failedAccessCount == counters_.failedAccessCount,
      checkpoint.version,
      counters_.version,
      checkpoint.values.size(),
      values_.size(),
      checkpoint.failedAccessCount,
      counters_.failedAccessCount};
  }

  bool rollbackToDebugCheckpoint(const ContainerDebugValueCheckpoint<T>& checkpoint,
                                 std::size_t expectedCurrentVersion,
                                 ContainerSourceLocation location = {})
  {
    if (checkpoint.source != this || counters_.version != expectedCurrentVersion) return false;
    const auto before = values_.size();
    values_ = checkpoint.values;
    recordMutation("debugRollback", before, values_.size());
    addDebugNote("restored values from debug checkpoint",
                 ContainerDebugNoteSeverity::Info,
                 ContainerDebugNoteAuthor::Runtime,
                 location);
    return true;
  }

  template <typename Callback>
  bool watch(const ContainerWatchRule& rule, Callback&& callback) const
  {
    const auto snapshot = debugSnapshot();
    const auto count = snapshot.info.count;
    const auto version = snapshot.counters.version;
    const auto readCount = snapshot.counters.readCount;
    const auto failedAccess = snapshot.counters.failedAccessCount > 0;
    const auto mutated = snapshot.counters.mutationCount > 0;
    const auto empty = snapshot.info.count == 0;
    const bool hit = (rule.minCount != 0 && count < rule.minCount)
      || (rule.maxCount != 0 && count > rule.maxCount)
      || (rule.minVersion != 0 && version < rule.minVersion)
      || (rule.maxVersion != 0 && version > rule.maxVersion)
      || (rule.minReadCount != 0 && readCount < rule.minReadCount)
      || (rule.maxReadCount != 0 && readCount > rule.maxReadCount)
      || (rule.watchEmpty && empty)
      || (rule.watchFailedAccess && failedAccess)
      || (rule.watchMutation && mutated);
    if (hit) {
      callback(ContainerWatchHit{"container-watch", createdAt_, snapshot});
    }
    return hit;
  }

  NamedVector<ContainerElementSample> debugSample(std::size_t limit = 4) const
  {
    NamedVector<ContainerElementSample> samples{ContainerName{"ContainerElementSamples"}};
    const auto sampleCount = values_.size() < limit ? values_.size() : limit;
    for (std::size_t index = 0; index < sampleCount; ++index) {
      samples.add(ContainerElementSample{
        index,
        static_cast<const void*>(&values_[index]),
        "sample"
      });
    }
    return samples;
  }

  void setDomain(ContainerDomain domain) noexcept
  {
    domain_ = domain;
  }

  ContainerDomain domain() const noexcept
  {
    return domain_;
  }

  void setOwner(ContainerOwner owner) noexcept
  {
    owner_ = owner;
  }

  ContainerOwner owner() const noexcept
  {
    return owner_;
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
    if (hit) callback(ContainerWatchHit{"container-changed-since-checkpoint", createdAt_, debugSnapshot()});
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

  auto begin() noexcept { return values_.begin(); }
  auto end() noexcept { return values_.end(); }
  auto begin() const noexcept { return values_.begin(); }
  auto end() const noexcept { return values_.end(); }

private:
  void recordMutation(const char* operation, std::size_t before, std::size_t after) noexcept
  {
    ++counters_.version;
    ++counters_.mutationCount;
    if (after > counters_.maxCountSeen) {
      counters_.maxCountSeen = after;
    }
    if (after > before) counters_.addedCount += after - before;
    if (before > after) counters_.removedCount += before - after;
    const auto currentCapacity = values_.capacity();
    if (currentCapacity != observedCapacity_) {
      ++counters_.capacityChangeCount;
      observedCapacity_ = currentCapacity;
    }
    if (currentCapacity > counters_.maxCapacitySeen) counters_.maxCapacitySeen = currentCapacity;
    const auto approximateBytes = currentCapacity * sizeof(T);
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

  std::vector<T> values_;
  ContainerName name_;
  ContainerDomain domain_ = ContainerDomain::Unknown;
  ContainerOwner owner_{};
  mutable ContainerDebugCounters counters_{};
  ContainerSourceLocation createdAt_{};
  ContainerSourceLocation lastMutatedAt_{};
  mutable ContainerSourceLocation lastFailedAccessAt_{};
  ContainerMutationRecord lastMutation_{};
  std::vector<ContainerMutationRecord> mutationHistory_;
  std::vector<ContainerDebugNote> debugNotes_;
  std::size_t observedCapacity_ = 0;
};

template <typename T>
NamedVector<T> makeNamedVector(ContainerName name)
{
  return NamedVector<T>(name);
}

template <typename T>
NamedVector<T> makeNamedVector(ContainerName name, ContainerSourceLocation createdAt)
{
  return NamedVector<T>(name, createdAt);
}

template <typename T>
NamedVector<T> makeNamedVector(const char* name)
{
  return NamedVector<T>(ContainerName{name});
}

template <typename T>
NamedVector<T> makeNamedVector(const char* name, ContainerSourceLocation createdAt)
{
  return NamedVector<T>(ContainerName{name}, createdAt);
}

}
