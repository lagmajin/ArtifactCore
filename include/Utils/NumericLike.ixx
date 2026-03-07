module;
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Numeric.Like;





export namespace ArtifactCore {

 template <typename T>
 concept NumericLike =
  requires(T a, T b) {
   { a + b } -> std::convertible_to<T>;
   { a - b } -> std::convertible_to<T>;
   { a* b } -> std::convertible_to<T>;
   { a / b } -> std::convertible_to<T>;
   { a = b };
   { static_cast<float>(a) }; // 暗黙または明示的変換ができること
 };

 template <typename T>
 concept FloatingLike = std::floating_point<T>;




};