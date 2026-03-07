module;
export module Particle;

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




export namespace ArtifactCore
{
 struct float2 {
  float x, y;
 };

 struct float3 {
  float x, y, z;
 };

 struct float4 {
  float x, y, z, w;
 };
	
 struct Particle {
  std::uint64_t id = 0;
  std::uint32_t seed = 0;
  std::uint32_t flags = 0;
  float age = 0.0f;
  float lifetime = 0.0f;

  float3 position{ 0.0f, 0.0f, 0.0f };
  float3 prevPosition{ 0.0f, 0.0f, 0.0f };
  float3 velocity{ 0.0f, 0.0f, 0.0f };
  float3 acceleration{ 0.0f, 0.0f, 0.0f };
  float3 rotation{ 0.0f, 0.0f, 0.0f };
  float3 angularVelocity{ 0.0f, 0.0f, 0.0f };

  float2 scale{ 1.0f, 1.0f };
  float size = 1.0f;
  float mass = 1.0f;
  float drag = 0.0f;
  float opacity = 1.0f;

  float4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
  float4 custom0{ 0.0f, 0.0f, 0.0f, 0.0f };
  float4 custom1{ 0.0f, 0.0f, 0.0f, 0.0f };

  // Rendering properties
  int textureIndex = -1;
  int blendMode = 0; // 0: Alpha, 1: Additive, 2: Screen, 3: Multiply

  // Sub-emitter tracking
  float lastSubEmitAge = 0.0f;
 };


};