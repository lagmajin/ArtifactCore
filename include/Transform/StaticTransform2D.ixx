module;
#include <stdint.h>
#include <QtGui/QtGui>

#include "../Define/DllExportMacro.hpp"

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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <opencv2/opencv.hpp>
export module Transform._2D;





namespace cv {};//dummy

export namespace ArtifactCore {


 using namespace cv;



 class LIBRARY_DLL_API StaticTransform2D {
 private:
  struct Impl;
  Impl* impl_;
  //std::unique_ptr<Impl> impl_2;
 public:
  StaticTransform2D();
  StaticTransform2D(const StaticTransform2D& other);
  StaticTransform2D(StaticTransform2D&& other) noexcept;
  ~StaticTransform2D();

  StaticTransform2D& operator=(const StaticTransform2D& other);
  StaticTransform2D& operator=(StaticTransform2D&& other) noexcept;

  float x() const;
  float y() const;
  float scaleX() const;
  float scaleY() const;
  void setX(float x);

  void setY(float y);
  float anchorPointX() const;
  float anchorPointY() const;

  void setScaleX(float x);
  void setScaleY(float y);

  void setAnchorPointX(float x);
  void setAnchorPointY(float y);

  void setInitialScaleX(float x);
  void setInitialScaleY(float y);
  void setInitialScale(float x, float y);
  void setTransform2D();
  QTransform toQTransform() const;

  float rotation() const;
  //Point2f anchor() const;
 };









};