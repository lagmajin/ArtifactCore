module;

#include <QString>
#include <DiligentCore/Common/interface/BasicMath.hpp>

#include "../Define/DllExportMacro.hpp"
export module Core.KeyFrame;

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




import Frame.Position;
import Math.Interpolate;

export namespace ArtifactCore {
	
 using namespace Diligent;
	
 using AnimValue = std::variant<
  float,
  uint32_t,
  float2,
  float4
 >;
	
	
 struct KeyFrame {
  FramePosition frame;  // int frame番号やフレーム単位の小数も扱える
  std::any value;// または std::variant<float, Vec2, ...>
  InterpolationType interpolation;
  float cp1_x, cp1_y;
  float cp2_x, cp2_y;
  //Diligent::float2
 };




 /*
 //keyframe class
 class  LIBRARY_DLL_API KeyFrame
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  KeyFrame();
  KeyFrame(const KeyFrame& frame);
  ~KeyFrame();
  float time() const;
  void setTime(float t);

  void setValue(const std::any& v);

 };
 */









};