module;

//#include "../third_party/Eigen/Core"
//#include "../third_party/Eigen/Dense"

#include <stdint.h>

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
export module Layer2D;





import ImageF32x4;
import Image;

import Transform;
import Layer.Blend;
import Layer.Matte;

export namespace ArtifactCore {

 

 class Layer2DSettingPrivate;

 class Layer2DSetting {
  
 public:

  //QPoint position;
  float opacity = 1.0f;
  BlendMode blendMode = BlendMode::Normal;
  MatteMode matteMode = MatteMode::None;
 };


 class Layer2D {
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
 public:
  Layer2D();
  ~Layer2D();
  
  // Transform2D 関連
  StaticTransform2D transform2D() const;
  ImageF32x4_RGBA transformedLayer();
  
  // Opacity 関連
  float opacity() const;
  void setOpacity(float value);
  
  // Blend/Matte 関連
  BlendMode blendMode() const;
  void setBlendMode(BlendMode mode);
  MatteMode matteMode() const;
  void setMatteMode(MatteMode mode);
 };




};