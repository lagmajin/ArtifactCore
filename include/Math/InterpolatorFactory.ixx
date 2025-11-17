module;
#include <QVector3D>
export module Interpolate.Factory;

import std;
import Math.Interpolate;

export namespace ArtifactCore {
 
 export template <typename T>
  class Interpolator
 {
 public:
  virtual ~Interpolator() = default;

  // t ∈ [0,1] の範囲で補間結果を返す
  virtual T interpolate(const T& a, const T& b, float t) const = 0;
 };

 template <typename T>
 using InterpolatorPtr = std::unique_ptr<Interpolator<T>>;

 export template <typename T>
  class LinearInterpolator final : public Interpolator<T>
 {
 public:
  T interpolate(const T& a, const T& b, float t) const override
  {
   return a + (b - a) * t;
  }
 };

 export template <typename T>
  class CubicInterpolator final : public Interpolator<T>
 {
 public:
  T interpolate(const T& a, const T& b, float t) const override
  {
   float t2 = t * t * (3 - 2 * t); // smoothstep式
   return a + (b - a) * t2;
  }
 };

 LinearInterpolator<float> linFloat;  // 型を明示してインスタンス化
 float x = linFloat.interpolate(0.0f, 10.0f, 0.5f);

 LinearInterpolator<QVector3D> linVec;  // QVector3D用
 QVector3D v = linVec.interpolate(QVector3D(0, 0, 0), QVector3D(1, 1, 1), 0.5f);



};