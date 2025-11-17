module;

export module Math.Interpolate;

import std;

export namespace ArtifactCore {
 
 using InterpFunc = std::function<float(float)>;

 export enum class InterpolationType {
  // 基本形
  Linear,          // 線形補間
  Constant,        // ステップ（補間なし）
  Smooth,          // 緩やかな補間（自動スムーズ）

  // 加速・減速系
  EaseIn,          // 徐々に加速
  EaseOut,         // 徐々に減速
  EaseInOut,       // 加速→減速
  EaseOutIn,       // 減速→加速（稀）

  // 二次・三次・指数系
  Quadratic,       // 二次曲線
  Cubic,           // 三次曲線
  Quartic,         // 四次
  Quintic,         // 五次
  Exponential,     // 指数的
  Logarithmic,     // 対数的

  // 円・三角・正弦
  Sine,            // 正弦波的
  Circular,        // 円弧状（sqrt補間）
  Cosine,          // 余弦波補間（主にAudioやWaveで使用）

  // バウンス・弾性系
  BounceIn,        // 弾むように入る
  BounceOut,       // 弾むように出る
  BounceInOut,     // 両端で弾む
  ElasticIn,       // ゴムのように伸びて入る
  ElasticOut,      // ゴムのように出る
  ElasticInOut,    // 両端で伸縮

  // バック・オーバーシュート系
  BackIn,          // 少し戻ってから進む
  BackOut,         // 行き過ぎて戻る
  BackInOut,       // 双方向オーバーシュート

  // Bézier・スプライン系
  Bezier,          // 制御点によるBézier補間
  CatmullRom,      // Catmull-Romスプライン
  Hermite,         // Hermite補間（Tangents指定あり）
  Barycentric,     // 三角補間（色空間やシェーディングで使用）

  // 物理系・特殊
  Spring,          // 物理スプリング感
  SmoothDamp,      // Unity系の減衰補間
  Step,            // 階段状
  CustomCurve,     // ユーザー定義カーブ
  Polynomial,      // 任意多項式
  Sigmoid,         // S字カーブ（AI・グラデーション向き）

  // 色補間・HDR向け
  GammaCorrected,  // ガンマ補正付き線形
  Perceptual,      // 人間知覚ベース補間（HDR / 色補間）
 };

 struct Linear {
  template<typename T>
  T operator()(const T& start, const T& end, float alpha) const {
   return start + (end - start) * alpha;
  }
 };

 // EaseIn補間
 struct EaseIn {
  template<typename T>
  T operator()(const T& start, const T& end, float alpha) const {
   alpha = alpha * alpha;
   return start + (end - start) * alpha;
  }
 };










};