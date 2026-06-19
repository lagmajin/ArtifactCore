module;
#include <algorithm>
#include <utility>
#include <vector>

export module Container.Algorithms;

import Container.NamedVector;

export namespace ArtifactCore {

template <typename T, typename F>
void each(const NamedVector<T>& values, F&& fn)
{
  values.each(std::forward<F>(fn));
}

template <typename T, typename Predicate>
std::size_t removeIf(NamedVector<T>& values, Predicate&& predicate)
{
  std::size_t removed = 0;
  std::size_t index = 0;
  while (index < values.count()) {
    const auto* value = values.at(index);
    if (value && predicate(*value)) {
      values.removeAt(index);
      ++removed;
      continue;
    }
    ++index;
  }
  return removed;
}

template <typename T>
std::vector<T> toStdVector(const NamedVector<T>& values)
{
  return values.toStdVector();
}

}
