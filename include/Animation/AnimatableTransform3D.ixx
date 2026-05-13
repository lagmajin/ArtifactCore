module;
#include <mfidl.h>
#include <DiligentCore/Common/interface/BasicMath.hpp>
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

export module Animation.Transform3D;

import Animation.Value;
import Animation.Transform2D;
import Time.Rational;
import Math.Interpolate;
import Property.Abstract;

export namespace ArtifactCore
{
 using namespace Diligent;

  struct Transform3DSnapshot {
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    float rotation = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float anchorX = 0.0f;
    float anchorY = 0.0f;
    float anchorZ = 0.0f;
    bool isZVisible = false;
    float4x4 matrix = float4x4::Identity();
    float4x4 matrixWithAnchor = float4x4::Identity();
    float2 anchorCanvasPosition = float2{0.0f, 0.0f};
  };

 class LIBRARY_DLL_API AnimatableTransform3D
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  AnimatableTransform3D();
  ~AnimatableTransform3D();

  AnimatableTransform3D(const AnimatableTransform3D& other);
  AnimatableTransform3D& operator=(const AnimatableTransform3D& other);
  AnimatableTransform3D(AnimatableTransform3D&& other) noexcept;
  AnimatableTransform3D& operator=(AnimatableTransform3D&& other) noexcept;

  void setInitialPosition(const RationalTime& time, float px, float py);
  void setInitialScale(const RationalTime& time, float xs, float ys);
  void setInitialRotation(const RationalTime& time, float angle);

  inline void setInitalAngle(const RationalTime& time, float angle) { setInitialRotation(time, angle); }

  void setPosition(const RationalTime& time, float x, float y);
  void setPositionZ(const RationalTime& time, float z);
  void setAnchor(const RationalTime& time, float x, float y, float z = 0.0f);
  void setRotation(const RationalTime& time, float degrees);
  void setScale(const RationalTime& time, float xs, float ys);

  size_t size() const;

  float positionX() const;
  float positionY() const;
  float positionZ() const;
  float rotation() const;
  float scaleX() const;
  float scaleY() const;
  float anchorX() const;
  float anchorY() const;
  float anchorZ() const;

  float positionXAt(const RationalTime& time) const;
  float positionYAt(const RationalTime& time) const;
  float positionZAt(const RationalTime& time) const;
  float rotationAt(const RationalTime& time) const;
  float scaleXAt(const RationalTime& time) const;
  float scaleYAt(const RationalTime& time) const;
  float anchorXAt(const RationalTime& time) const;
  float anchorYAt(const RationalTime& time) const;
  float anchorZAt(const RationalTime& time) const;
  float2 anchorPosition() const;
  float2 anchorPositionAt(const RationalTime& time) const;

  float4x4 getMatrix() const;
  float4x4 getAllMatrix() const;
  float4x4 getMatrixAt(const RationalTime& time) const;
  float4x4 getAllMatrixAt(const RationalTime& time) const;
  Transform3DSnapshot snapshot() const;
  Transform3DSnapshot snapshotAt(const RationalTime& time) const;
  
  bool hasPositionKeyFrameAt(const RationalTime& time) const;
  bool hasRotationKeyFrameAt(const RationalTime& time) const;
  bool hasScaleKeyFrameAt(const RationalTime& time) const;
  bool setPositionKeyFrameValueAt(const RationalTime& time, float x, float y);
  bool setPositionKeyFrameInterpolationAt(const RationalTime& time,
                                         InterpolationType xInterpolation,
                                         InterpolationType yInterpolation);
  InterpolationType positionXKeyFrameInterpolationAt(const RationalTime& time) const;
  InterpolationType positionYKeyFrameInterpolationAt(const RationalTime& time) const;
  
  void removePositionKeyFrameAt(const RationalTime& time);
  void removeRotationKeyFrameAt(const RationalTime& time);
  void removeScaleKeyFrameAt(const RationalTime& time);
  
  void clearPositionKeyFrames();
  void clearRotationKeyFrames();
  void clearScaleKeyFrames();
  void clearAllKeyFrames();
  
  size_t getPositionKeyFrameCount() const;
  size_t getRotationKeyFrameCount() const;
  size_t getScaleKeyFrameCount() const;
  
  std::vector<RationalTime> getPositionKeyFrameTimes() const;
  std::vector<RationalTime> getRotationKeyFrameTimes() const;
  std::vector<RationalTime> getScaleKeyFrameTimes() const;
  std::vector<RationalTime> getAllKeyFrameTimes() const;
 };

};
