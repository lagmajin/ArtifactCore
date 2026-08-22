module;
#include <cstddef>
#include <type_traits>
#include <utility>

export module Core.ArtifactAlgorithms;

import Core.ArtifactUtility;

export namespace ArtifactCore {

// Self-contained <algorithm> replacements operating on iterator pairs or
// whole containers exposing begin()/end(). artifactSort is heapsort: in
// place, O(n log n), non-stable — matching std::sort guarantees.

namespace detail {
template <typename Iterator, typename Compare>
void artifactSiftDown(Iterator first, std::ptrdiff_t start,
                      std::ptrdiff_t end, Compare& compare) {
    std::ptrdiff_t root = start;
    while (true) {
        std::ptrdiff_t child = root * 2 + 1;
        if (child > end) break;
        if (child + 1 <= end && compare(first[child], first[child + 1])) {
            ++child;
        }
        if (!compare(first[root], first[child])) break;
        auto tmp = artifactMove(first[root]);
        first[root] = artifactMove(first[child]);
        first[child] = artifactMove(tmp);
        root = child;
    }
}
} // namespace detail

// ---- non-modifying ----

template <typename Iterator, typename T>
Iterator artifactFind(Iterator first, Iterator last, const T& value) {
    for (; first != last; ++first) {
        if (*first == value) break;
    }
    return first;
}

template <typename Iterator, typename Predicate>
Iterator artifactFindIf(Iterator first, Iterator last, Predicate&& predicate) {
    for (; first != last; ++first) {
        if (predicate(*first)) break;
    }
    return first;
}

template <typename Container, typename T>
bool artifactContains(const Container& container, const T& value) {
    for (const auto& element : container) {
        if (element == value) return true;
    }
    return false;
}

template <typename Iterator, typename Predicate>
bool artifactAllOf(Iterator first, Iterator last, Predicate&& predicate) {
    for (; first != last; ++first) {
        if (!predicate(*first)) return false;
    }
    return true;
}

template <typename Iterator, typename Predicate>
bool artifactAnyOf(Iterator first, Iterator last, Predicate&& predicate) {
    return artifactFindIf(first, last, predicate) != last;
}

template <typename Iterator, typename Predicate>
bool artifactNoneOf(Iterator first, Iterator last, Predicate&& predicate) {
    return !artifactAnyOf(first, last, predicate);
}

template <typename Iterator>
Iterator artifactMinElement(Iterator first, Iterator last) {
    if (first == last) return last;
    Iterator smallest = first;
    for (++first; first != last; ++first) {
        if (*first < *smallest) smallest = first;
    }
    return smallest;
}

template <typename Iterator>
Iterator artifactMaxElement(Iterator first, Iterator last) {
    if (first == last) return last;
    Iterator largest = first;
    for (++first; first != last; ++first) {
        if (*largest < *first) largest = first;
    }
    return largest;
}

// Returns {minElement, maxElement}. First == second for empty ranges.
template <typename Iterator>
Pair<Iterator, Iterator> artifactMinMaxElement(Iterator first, Iterator last) {
    return {artifactMinElement(first, last), artifactMaxElement(first, last)};
}

template <typename Iterator, typename T, typename BinaryOp>
T artifactAccumulate(Iterator first, Iterator last, T init, BinaryOp&& op) {
    for (; first != last; ++first) {
        init = op(init, *first);
    }
    return init;
}

template <typename Iterator, typename T>
T artifactAccumulate(Iterator first, Iterator last, T init) {
    for (; first != last; ++first) {
        init = artifactMove(init) + *first;
    }
    return init;
}

template <typename Iterator, typename T>
void artifactIota(Iterator first, Iterator last, const T& startValue) {
    T current = startValue;
    for (; first != last; ++first, static_cast<void>(++current)) {
        *first = current;
    }
}

template <typename Iterator, typename T>
std::ptrdiff_t artifactCount(Iterator first, Iterator last, const T& value) {
    std::ptrdiff_t total = 0;
    for (; first != last; ++first) {
        if (*first == value) ++total;
    }
    return total;
}

template <typename Iterator, typename Predicate>
std::ptrdiff_t artifactCountIf(Iterator first, Iterator last, Predicate&& predicate) {
    std::ptrdiff_t total = 0;
    for (; first != last; ++first) {
        if (predicate(*first)) ++total;
    }
    return total;
}

// ---- modifying ----

template <typename Iterator, typename T>
void artifactFill(Iterator first, Iterator last, const T& value) {
    for (; first != last; ++first) *first = value;
}

template <typename Iterator>
void artifactReverse(Iterator first, Iterator last) {
    if (first == last) return;
    --last;
    while (first < last) {
        auto tmp = artifactMove(*first);
        *first = artifactMove(*last);
        *last = artifactMove(tmp);
        ++first;
        --last;
    }
}

// Erase-remove for containers with eraseAt(size_t) style removal. Returns
// the new element count.
template <typename Container, typename Predicate>
std::size_t artifactRemoveIf(Container& container, Predicate&& predicate) {
    std::size_t out = 0;
    const std::size_t count = static_cast<std::size_t>(container.end() - container.begin());
    for (std::size_t index = 0; index < count; ++index) {
        if (!predicate(container[index])) {
            if (out != index) container[out] = artifactMove(container[index]);
            ++out;
        }
    }
    for (std::size_t index = count; index > out; --index) {
        container.eraseAt(index - 1);
    }
    return out;
}

// ---- sorted-range ops ----

template <typename Iterator, typename T>
Iterator artifactLowerBound(Iterator first, Iterator last, const T& value) {
    Iterator it = first;
    std::ptrdiff_t count = last - first;
    while (count > 0) {
        const std::ptrdiff_t step = count / 2;
        Iterator probe = it + step;
        if (*probe < value) {
            it = ++probe;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return it;
}

template <typename Iterator, typename T>
bool artifactBinarySearch(Iterator first, Iterator last, const T& value) {
    const Iterator position = artifactLowerBound(first, last, value);
    return position != last && *position == value;
}

template <typename Iterator>
bool artifactIsSorted(Iterator first, Iterator last) {
    if (first == last) return true;
    Iterator previous = first;
    ++first;
    for (; first != last; ++first) {
        if (*first < *previous) return false;
        previous = first;
    }
    return true;
}

// ---- sorting ----

template <typename Iterator, typename Compare>
void artifactSort(Iterator first, Iterator last, Compare&& compare) {
    const std::ptrdiff_t count = last - first;
    if (count < 2) return;
    const std::ptrdiff_t endIndex = count - 1;
    for (std::ptrdiff_t start = count / 2 - 1; start >= 0; --start) {
        detail::artifactSiftDown(first, start, endIndex, compare);
    }
    for (std::ptrdiff_t end = endIndex; end > 0; --end) {
        auto tmp = artifactMove(first[0]);
        first[0] = artifactMove(first[end]);
        first[end] = artifactMove(tmp);
        detail::artifactSiftDown(first, 0, end - 1, compare);
    }
}

template <typename Iterator>
void artifactSort(Iterator first, Iterator last) {
    artifactSort(first, last,
                 [](const auto& a, const auto& b) { return a < b; });
}

// ---- container conveniences ----

template <typename Container>
void artifactSort(Container& container) {
    artifactSort(container.begin(), container.end());
}

template <typename Container, typename Compare>
void artifactSort(Container& container, Compare&& compare) {
    artifactSort(container.begin(), container.end(),
                 static_cast<Compare&&>(compare));
}

template <typename Container, typename T>
auto artifactFindIn(Container& container, const T& value) {
    return artifactFind(container.begin(), container.end(), value);
}

} // namespace ArtifactCore
