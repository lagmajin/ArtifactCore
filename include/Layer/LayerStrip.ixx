
module;
#include <cstdint>
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
export module Layer.LayerStrip;

export namespace ArtifactCore {

 class LayerStrip {
 private:
  class Impl;
  Impl* impl_;
 public:
  LayerStrip();
  LayerStrip(int32_t startFrame, int32_t endFrame);
  ~LayerStrip();

  // In/Out point setters (existing API)
  void SetStartFrame(int32_t frame);
  void SetEndFrame(int32_t frame);

  // Convenience
  void SetRange(int32_t startFrame, int32_t endFrame);

  // In/Out point getters
  int32_t StartFrame() const;
  int32_t EndFrame() const;

  // Duration (end - start)
  int32_t Duration() const;

  // Validation
  bool IsValid() const;  // start <= end

  // Frame containment test
  bool ContainsFrame(int32_t frame) const;

  // Shift the entire strip
  void Shift(int32_t offset);
 };

};