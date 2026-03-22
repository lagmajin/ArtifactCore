module;
#include <type_traits>
#include <opencv2/core/types.hpp>
#include <QPointF>
#include <QVector2D>
#include <glm/glm.hpp>
#include "../Define/DllExportMacro.hpp"
#include "../ArtifactWidgets/include/Define/DllExportMacro.hpp"
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
export module Core.Point2D;






export namespace ArtifactCore {

 template<typename T>
 concept Point2DF = requires(T p) {
  { getX(p) } -> std::convertible_to<float>;
  { getY(p) } -> std::convertible_to<float>;
 };


 inline float getX(const cv::Point2f& p) { return p.x; }
 inline float getY(const cv::Point2f& p) { return p.y; }


 inline float getX(const QPointF& p) { return static_cast<float>(p.x()); }
 inline float getY(const QPointF& p) { return static_cast<float>(p.y()); }




 class LIBRARY_DLL_API Point2DF{
 private:
  class Impl;
  Impl* impl_;
 public:
  Point2DF();
  ~Point2DF();
  float getX() const;
  float getY() const;
 
 };

 template<typename T>
 concept Point2DI = requires(T p) {
  { getX(p) } -> std::convertible_to<int32_t>;
  { getY(p) } -> std::convertible_to<int32_t>;
 };

 inline int32_t getX(const cv::Point& p) { return p.x; }
 inline int32_t getY(const cv::Point& p) { return p.y; }

 inline int32_t getX(const QPoint& p) { return p.x(); }
 inline int32_t getY(const QPoint& p) { return p.y(); }


 class LIBRARY_DLL_API Point2DI {
 private:
  class Impl;
  Impl* impl_;
 public:
  Point2DI();
  ~Point2DI();

  int32_t getX() const;
  int32_t getY() const;
 };

}