module ;
#include <mfidl.h>
#include <DiligentCore/Common/interface/BasicMath.hpp>

#include "../Define/DllExportMacro.hpp"
export module Animation.Transform3D;

import std;
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
  void setInitialScale(const RationalTime& time,float xs,float ys);
  void setInitalAngle(const RationalTime& time,float angle=0);
  void setUserScale(const RationalTime& time,float xs,float ys);
  void setInitialPosition(const RationalTime& time,float px,float py);
  void setPosition(const RationalTime& time,float x, float y);
  void setRotation(const RationalTime& time,float degrees);
  void setScale(const RationalTime& time,float xs,float ys);
 	
  size_t size() const;

  // 現在値の取得（最後に設定された値）
  float positionX() const;
  float positionY() const;
  float rotation() const;
  float scaleX() const;
  float scaleY() const;
  
  // 指定時刻のアニメーション補間値を取得（UI用）
  float positionXAt(const RationalTime& time) const;
  float positionYAt(const RationalTime& time) const;
  float rotationAt(const RationalTime& time) const;
  float scaleXAt(const RationalTime& time) const;
  float scaleYAt(const RationalTime& time) const;
  
  // 4x4変換行列の取得（レンダリング用）
  // 現在値から行列を生成
  float4x4 getMatrix() const;
  
  // 指定時刻のアニメーション補間値から行列を生成
  float4x4 getMatrixAt(const RationalTime& time) const;
  
  // ============================================
  // キーフレーム管理機能（UI/エディタ用）
  // ============================================
  
  // 指定時刻にキーフレームが存在するか
  bool hasPositionKeyFrameAt(const RationalTime& time) const;
  bool hasRotationKeyFrameAt(const RationalTime& time) const;
  bool hasScaleKeyFrameAt(const RationalTime& time) const;
  
  // 指定時刻のキーフレームを削除
  void removePositionKeyFrameAt(const RationalTime& time);
  void removeRotationKeyFrameAt(const RationalTime& time);
  void removeScaleKeyFrameAt(const RationalTime& time);
  
  // すべてのキーフレームをクリア
  void clearPositionKeyFrames();
  void clearRotationKeyFrames();
  void clearScaleKeyFrames();
  void clearAllKeyFrames();
  
  // キーフレーム数を取得
  size_t getPositionKeyFrameCount() const;
  size_t getRotationKeyFrameCount() const;
  size_t getScaleKeyFrameCount() const;
  
  // すべてのキーフレーム時刻を取得（ソート済み）
  std::vector<RationalTime> getPositionKeyFrameTimes() const;
  std::vector<RationalTime> getRotationKeyFrameTimes() const;
  std::vector<RationalTime> getScaleKeyFrameTimes() const;
 };

 };