module;
#define QT_NO_KEYWORDS
#include <QString>

export module Color.Saturation;

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



import Utils.String.UniString;


export namespace ArtifactCore {

	
	
 class Saturation
 {
 private:
  class Impl;
  Impl* impl_;

 public:
  Saturation();
  explicit Saturation(float s);
  ~Saturation();
  float saturation() const;
  void setSaturation(float s); // 0..1 にクランプ

  //operator ==
  bool operator==(const Saturation& other) const;
  
  bool operator!=(const Saturation& other) const;

  bool operator>(const Saturation& other) const
  {
   return saturation() > other.saturation();
  }

  //operator >=
  bool operator>=(const Saturation& other) const
  {
   return saturation() >= other.saturation();
  }

   
  bool operator<(const Saturation& other) const
  {
   return saturation() < other.saturation();
  }

 };

};

