import io

p = 'include/Container/NamedList.ixx'
s = io.open(p, encoding='utf-8').read()

repl = [
    ('return values_.empty();', 'return values_.isEmpty();'),
    ('values_.empty()', 'values_.isEmpty()'),
    ('values_.clear();', 'values_.removeAll();'),
    ('values_.push_back(value);', 'values_.append(value);'),
    ('values_.push_back(std::move(value));', 'values_.append(std::move(value));'),
    ('values_.push_front(value);', 'values_.prepend(value);'),
    ('values_.push_front(std::move(value));', 'values_.prepend(std::move(value));'),
    ('values_.assign(values.begin(), values.end());',
     'values_.removeAll();\n    for (const auto& item : values) values_.append(item);'),
    ('auto& value = values_.emplace_back(std::forward<Args>(args)...);\n    recordMutation("make", before, values_.size());\n    return value;',
     'values_.append(T(std::forward<Args>(args)...));\n    T& value = values_.last();\n    recordMutation("make", before, values_.size());\n    return value;'),
    ('return &values_.front();', 'return &values_[0];'),
    ('return &values_.back();', 'return &values_[values_.size() - 1];'),
    ('values_.pop_front();', 'values_.removeFirst();'),
    ('values_.pop_back();', 'values_.removeLast();'),
    ('Array<ContainerMutationRecord> mutationHistory_;', 'Array<ContainerMutationRecord> mutationHistory_;'),
    ('std::list<T> values_;', 'Array<T> values_;'),
]

for a, b in repl:
    if a == b:
        continue
    n = s.count(a)
    s = s.replace(a, b)
    print('%3d  %s' % (n, a.splitlines()[0][:70]))

io.open(p, 'w', encoding='utf-8', newline='').write(s)
print('written')