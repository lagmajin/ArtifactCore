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
export module Layer2D;





import ImageF32x4;
import Image;

import Transform;

export namespace ArtifactCore {

 

 class Layer2DSettingPrivate;

 class Layer2DSetting {
  
 public:

  //QPoint position;
  float opacity = 1.0f;
 };


 class Layer2D {
 private:
  struct Impl;                
  std::unique_ptr<Impl> impl_;
 public:
  Layer2D();
  ~Layer2D();
  StaticTransform2D transform2D() const;
  ImageF32x4_RGBA transformedLayer();
 };




};