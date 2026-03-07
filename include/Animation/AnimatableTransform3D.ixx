module ;
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

export module Animation.Transform3D;




import Animation.Value;
import Animation.Transform2D;
import Time.Rational;
import Math.Interpolate;
import Property.Abstract;

export namespace ArtifactCore
{
 using namespace Diligent;


 class LIBRARY_DLL_API AnimatableTransform3D
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  AnimatableTransform3D();
  ~AnimatableTransform3D();
  void setInitialPosition(const RationalTime& time, float px, float py);
  void setInitialScale(const RationalTime& time, float xs, float ys);
  void setInitialRotation(const RationalTime& time, float angle);

  // Legacy alias
  void setInitalAngle(const RationalTime& time, float angle) { setInitialRotation(time, angle); }

  void setPosition(const RationalTime& time, float x, float y);
  void setPositionZ(const RationalTime& time, float z);
  void setAnchor(const RationalTime& time, float x, float y, float z = 0.0f);
  void setRotation(const RationalTime& time, float degrees);
  void setScale(const RationalTime& time, float xs, float ys);

  size_t size() const;

  // Returns combined (Baseline + Offset) values for current time
  float positionX() const;
  float positionY() const;
  float positionZ() const;
  float rotation() const;
  float scaleX() const;
  float scaleY() const;
  float anchorX() const;
  float anchorY() const;
  float anchorZ() const;

  // Returns combined (Baseline + Offset) values at specific time
  float positionXAt(const RationalTime& time) const;
  float positionYAt(const RationalTime& time) const;
  float positionZAt(const RationalTime& time) const;
  float rotationAt(const RationalTime& time) const;
  float scaleXAt(const RationalTime& time) const;
  float scaleYAt(const RationalTime& time) const;
  float anchorXAt(const RationalTime& time) const;
  float anchorYAt(const RationalTime& time) const;
  float anchorZAt(const RationalTime& time) const;

  // Returns the final world matrix (Baseline * Offset)
  float4x4 getMatrix() const;
  float4x4 getAllMatrix() const;
  float4x4 getMatrixAt(const RationalTime& time) const;
  float4x4 getAllMatrixAt(const RationalTime& time) const;
  
  // ============================================
  // �L�[�t���[���Ǘ��@�\�iUI/�G�f�B�^�p�j
  // ============================================
  
  // �w�莞���ɃL�[�t���[�������݂��邩
  bool hasPositionKeyFrameAt(const RationalTime& time) const;
  bool hasRotationKeyFrameAt(const RationalTime& time) const;
  bool hasScaleKeyFrameAt(const RationalTime& time) const;
  
  // �w�莞���̃L�[�t���[�����폜
  void removePositionKeyFrameAt(const RationalTime& time);
  void removeRotationKeyFrameAt(const RationalTime& time);
  void removeScaleKeyFrameAt(const RationalTime& time);
  
  // ���ׂẴL�[�t���[�����N���A
  void clearPositionKeyFrames();
  void clearRotationKeyFrames();
  void clearScaleKeyFrames();
  void clearAllKeyFrames();
  
  // �L�[�t���[�������擾
  size_t getPositionKeyFrameCount() const;
  size_t getRotationKeyFrameCount() const;
  size_t getScaleKeyFrameCount() const;
  
  // ���ׂẴL�[�t���[���������擾�i�\�[�g�ς݁j
  std::vector<RationalTime> getPositionKeyFrameTimes() const;
  std::vector<RationalTime> getRotationKeyFrameTimes() const;
  std::vector<RationalTime> getScaleKeyFrameTimes() const;
 };

 };