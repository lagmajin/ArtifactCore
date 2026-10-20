import io

p = 'include/Container/NameMap.ixx'
s = io.open(p, encoding='utf-8').read()

repl = [
    ('return values_.empty();', 'return values_.empty();'),
    ('values_.clear();', 'values_.clear();'),
    # tryTake: erase-by-iterator is unavailable; use findValue + erase-by-key.
    ("""    auto it = values_.find(key);
    if (it == values_.end()) {
      bumpFailedAccess();
      return false;
    }
    const auto before = values_.size();
    result = std::move(it->second);
    values_.erase(it);
    recordMutation("tryTake", before, values_.size());
    return true;""",
     """    V* found = values_.findValue(key);
    if (found == nullptr) {
      bumpFailedAccess();
      return false;
    }
    const auto before = values_.size();
    result = std::move(*found);
    values_.erase(key);
    recordMutation("tryTake", before, values_.size());
    return true;"""),
    # tryEmplace: std::map::try_emplace -> ArtifactHashMap::tryEmplace returns pair<iterator,bool>
    ("auto [it, inserted] = values_.try_emplace(key, std::forward<Args>(args)...);\n"
     "    if (inserted) recordMutation(\"tryEmplace\", before, values_.size());\n"
     "    return {&it->second, inserted};",
     "auto [it, inserted] = values_.tryEmplace(key, std::forward<Args>(args)...);\n"
     "    if (inserted) recordMutation(\"tryEmplace\", before, values_.size());\n"
     "    return {&(it->second), inserted};"),
    # toStdMap must materialise a real std::map now that values_ is a hash map.
    ("""  std::map<K, V> toStdMap() const
  {
    return values_;
  }""",
     """  std::map<K, V> toStdMap() const
  {
    std::map<K, V> result;
    for (const auto& entry : values_) {
      result.emplace(entry.first, entry.second);
    }
    return result;
  }"""),
]

for a, b in repl:
    n = s.count(a)
    if n == 0:
        print('MISS  %r' % (a[:60],))
    s = s.replace(a, b)
    print('%3d  %s' % (n, a.strip().splitlines()[0][:70] if a.strip() else ''))

io.open(p, 'w', encoding='utf-8', newline='').write(s)
print('written')