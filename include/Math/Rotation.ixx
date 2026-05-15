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
export module Math.Rotation;

export namespace ArtifactCore
{
 class Rotation
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  Rotation();
  Rotation(const Rotation& other);
  Rotation(Rotation&& other) noexcept;
  ~Rotation();
 	
  Rotation& operator=(const Rotation& other);
 	
 };
	
	
	
};
