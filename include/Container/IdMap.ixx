module;
#include <cstddef>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

export module Container.IdMap;

import Container.Debug;
import Container.NamedVector;

export namespace ArtifactCore {

template <typename K, typename V>
class IdMap {
public:
  using Key = K;
  using Value = V;

  IdMap() = default;

  explicit IdMap(ContainerName name) noexcept
    : name_(name)
  {
  }

  IdMap(ContainerName name, ContainerSourceLocation createdAt) noexcept
    : name_(name)
    , createdAt_(createdAt)
  {
  }

  IdMap(ContainerName name, ContainerDomain domain) noexcept
    : name_(name)
    , domain_(domain)
  {
  }

  IdMap(ContainerName name, ContainerDomain domain, ContainerSourceLocation createdAt) noexcept
    : name_(name)
    , domain_(domain)
    , createdAt_(createdAt)
  {
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

  bool contains(const K& key) const
  {
    ++counters_.readCount;
    return values_.find(key) != values_.end();
  }

  std::size_t capacity() const noexcept
  {
    ++counters_.readCount;
    return values_.bucket_count();
  }

  void reserve(std::size_t count)
  {
    const auto before = values_.size();
    values_.reserve(count);
    recordMutation("reserve", before, values_.size());
  }

  void squeeze()
  {
    const auto before = values_.size();
    values_.rehash(0);
    recordMutation("squeeze", before, values_.size());
  }

  V* at(const K& key)
  {
    ++counters_.readCount;
    auto it = values_.find(key);
    if (it == values_.end()) {
      bumpFailedAccess();
      return nullptr;
    }
    return &it->second;
  }

  const V* at(const K& key) const
  {
    ++counters_.readCount;
    auto it = values_.find(key);
    if (it == values_.end()) {
      bumpFailedAccess();
      return nullptr;
    }
    return &it->second;
  }

  V& operator[](const K& key)
  {
    const auto before = values_.size();
    auto& value = values_[key];
    if (values_.size() > before) {
      updateMaxCount();
      recordMutation("operator[]", before, values_.size());
    }
    return value;
  }

  void insert(const K& key, const V& value)
  {
    const auto before = values_.size();
    values_[key] = value;
    if (values_.size() > before) {
      updateMaxCount();
    }
    recordMutation("insert", before, values_.size());
  }

  void add(const K& key, const V& value)
  {
    insert(key, value);
  }

  V value(const K& key, V defaultValue = V{}) const
  {
    ++counters_.readCount;
    const auto it = values_.find(key);
    return it == values_.end() ? std::move(defaultValue) : it->second;
  }

  template <typename... Args>
  std::pair<V*, bool> tryEmplace(const K& key, Args&&... args)
  {
    const auto before = values_.size();
    auto [it, inserted] = values_.try_emplace(key, std::forward<Args>(args)...);
    if (inserted) recordMutation("tryEmplace", before, values_.size());
    return {&it->second, inserted};
  }

  bool tryTake(const K& key, V& result)
  {
    auto it = values_.find(key);
    if (it == values_.end()) {
      bumpFailedAccess();
      return false;
    }
    const auto before = values_.size();
    result = std::move(it->second);
    values_.erase(it);
    recordMutation("tryTake", before, values_.size());
    return true;
  }

  template <typename Predicate>
  std::size_t removeIf(Predicate&& predicate)
  {
    const auto before = values_.size();
    for (auto it = values_.begin(); it != values_.end();) {
      if (predicate(it->first, it->second)) it = values_.erase(it);
      else ++it;
    }
    const auto removed = before - values_.size();
    if (removed != 0) recordMutation("removeIf", before, values_.size());
    return removed;
  }

  bool remove(const K& key)
  {
    const auto before = values_.size();
    const auto erased = values_.erase(key);
    if (erased > 0) {
      recordMutation("remove", before, values_.size());
      return true;
    }
    bumpFailedAccess();
    return false;
  }

  void clear() noexcept
  {
    const auto before = values_.size();
    values_.clear();
    recordMutation("clear", before, 0);
  }

  template <typename F>
  void eachKeyValue(F&& fn) const
  {
    for (const auto& [key, value] : values_) {
      fn(key, value);
    }
  }

  template <typename F>
  void mutableEachKeyValue(F&& fn)
  {
    for (auto& [key, value] : values_) {
      fn(key, value);
    }
  }

  NamedVector<K> keys() const
  {
    NamedVector<K> result(name_, domain_);
    result.reserve(values_.size());
    for (const auto& [key, value] : values_) {
      result.add(key);
    }
    return result;
  }

  NamedVector<V> values() const
  {
    NamedVector<V> result(name_, domain_);
    result.reserve(values_.size());
    for (const auto& [key, value] : values_) {
      result.add(value);
    }
    return result;
  }

  std::unordered_map<K, V> toStdUnorderedMap() const
  {
    return values_;
  }

  auto begin() const noexcept { return values_.begin(); }
  auto end() const noexcept { return values_.end(); }
  auto begin() noexcept { return values_.begin(); }
  auto end() noexcept { return values_.end(); }

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
    if (hit) callback(ContainerWatchHit{"idmap-changed-since-checkpoint", createdAt_, debugSnapshot()});
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
      typeid(V).name(),
      values_.size(),
      values_.bucket_count(),
      values_.bucket_count() * sizeof(void*) + values_.size() * (sizeof(K) + sizeof(V) + sizeof(std::size_t))
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
    NamedVector<ContainerElementSample> samples{ContainerName{"IdMapElementSamples"}};
    std::size_t index = 0;
    for (const auto& [key, value] : values_) {
      if (index >= limit) break;
      samples.add(ContainerElementSample{index, static_cast<const void*>(&value), "sample"});
      ++index;
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
      callback(ContainerWatchHit{"idmap-watch", createdAt_, snapshot});
    }
    return hit;
  }

private:
  void recordMutation(const char* operation, std::size_t before, std::size_t after) noexcept
  {
    ++counters_.version;
    ++counters_.mutationCount;
    if (after > counters_.maxCountSeen) counters_.maxCountSeen = after;
    if (after > before) counters_.addedCount += after - before;
    if (before > after) counters_.removedCount += before - after;
    const auto currentCapacity = values_.bucket_count();
    if (currentCapacity != observedCapacity_) {
      ++counters_.capacityChangeCount;
      observedCapacity_ = currentCapacity;
    }
    if (currentCapacity > counters_.maxCapacitySeen) counters_.maxCapacitySeen = currentCapacity;
    const auto approximateBytes = currentCapacity * sizeof(void*) + after * (sizeof(K) + sizeof(V) + sizeof(std::size_t));
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

  void updateMaxCount() noexcept
  {
    if (values_.size() > counters_.maxCountSeen) {
      counters_.maxCountSeen = values_.size();
    }
  }

  std::unordered_map<K, V> values_;
  ContainerName name_;
  ContainerDomain domain_ = ContainerDomain::Unknown;
  ContainerOwner owner_{};
  mutable ContainerDebugCounters counters_{};
  ContainerSourceLocation createdAt_{};
  ContainerSourceLocation lastMutatedAt_{};
  mutable ContainerSourceLocation lastFailedAccessAt_{};
  ContainerMutationRecord lastMutation_{};
  std::vector<ContainerMutationRecord> mutationHistory_;
  std::size_t observedCapacity_ = 0;
};

template <typename K, typename V>
IdMap<K, V> makeIdMap(ContainerName name)
{
  return IdMap<K, V>(name);
}

template <typename K, typename V>
IdMap<K, V> makeIdMap(ContainerName name, ContainerSourceLocation location)
{
  return IdMap<K, V>(name, location);
}

template <typename K, typename V>
IdMap<K, V> makeIdMap(const char* name)
{
  return IdMap<K, V>(ContainerName{name});
}

template <typename K, typename V>
IdMap<K, V> makeIdMap(const char* name, ContainerSourceLocation location)
{
  return IdMap<K, V>(ContainerName{name}, location);
}

}
