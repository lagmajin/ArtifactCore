module ;
export module Animation.Transform2D;

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



import Animation.Value;



export namespace ArtifactCore {

 class AnimatableTransform2D {
 private:
  class Impl;
  Impl* impl_;

 public:
  enum class Element {
   Position,
   Rotation,
   Scale,
   Count  // vfp
  };

  void setPosition(float x, float y);

  // Rotation
  void setRotation(float degrees);

  // Scale
  void setScale(float sx, float sy);
  size_t size() const;
 	
 };




};