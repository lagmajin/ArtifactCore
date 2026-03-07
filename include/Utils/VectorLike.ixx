module;
#include <QVector2D>
#include <QVector3D>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <opencv2/core.hpp>
export module Vector.Like;

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




export namespace ArtifactCore {

 template <typename T>
 concept VectorLike = requires(T v) {
  { v.x } -> std::convertible_to<float>;
  { v.y } -> std::convertible_to<float>;
 };




};