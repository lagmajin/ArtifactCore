module;
#include "../Define/DllExportMacro.hpp"
#include <QSize>

export module Color.LUT;

import std;
import Utils.String.UniString;
import FloatRGBA;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

 // LUTのタイプ
 enum class LUTType {
  Unknown,
  LUT1D,        // 1次元LUT
  LUT3D         // 3次元LUT
 };

 // LUTフォーマット
 enum class LUTFormat {
  Unknown,
  Cube,         // .cube (Adobe)
  ThreeDL,      // .3dl (Autodesk)
  ICC,          // .icc/.icm (ICC Profile)
  CSP,          // .csp (Rising Sun Research)
  Look,         // .look (IRIDAS)
  Mga,          // .mga (Pandora)
  Json,         // .json (カスタム)
  Binary        // .lut (バイナリ)
 };

 // LUT補間モード
 enum class LUTInterpolation {
  Nearest,      // 最近傍
  Trilinear,    // 三線形補間
  Tetrahedral   // 四面体補間（最高品質）
 };

 // LUT情報
 struct LUTInfo {
  LUTType type = LUTType::Unknown;
  LUTFormat format = LUTFormat::Unknown;
  int size = 0;                     // 1D: サンプル数、3D: 各軸のサイズ
  UniString title;
  UniString description;
  bool isIdentity = false;          // 恒等変換か
  float inputMin = 0.0f;            // 入力範囲
  float inputMax = 1.0f;
  float outputMin = 0.0f;           // 出力範囲
  float outputMax = 1.0f;
 };

 // LUT読み込み結果
 struct LUTLoadResult {
  bool success = false;
  LUTInfo info;
  UniString errorMessage;
 };

 // LUT適用オプション
 struct LUTApplyOptions {
  LUTInterpolation interpolation = LUTInterpolation::Tetrahedral;
  float intensity = 1.0f;           // 適用強度（0.0-1.0）
  bool clampOutput = true;          // 出力を範囲内にクランプ
  bool preserveAlpha = true;        // アルファチャンネルを保持
 };

 class LIBRARY_DLL_API LUT {
 private:
  class Impl;
  Impl* impl_;

 public:
  LUT();
  ~LUT();

  // コピー/ムーブ
  LUT(const LUT&) = delete;
  LUT& operator=(const LUT&) = delete;
  LUT(LUT&&) noexcept;
  LUT& operator=(LUT&&) noexcept;

  // ---- 読み込み/保存 ----

  // ファイルからLUTを読み込み
  LUTLoadResult load(const UniString& filePath);
  
  // ファイルからLUTを読み込み（フォーマット指定）
  LUTLoadResult load(const UniString& filePath, LUTFormat format);
  
  // メモリからLUTを読み込み
  bool loadFromMemory(const std::vector<float>& data, int size, LUTType type);
  
  // LUTを保存
  bool save(const UniString& filePath, LUTFormat format = LUTFormat::Cube);

  // ---- LUT生成 ----

  // 恒等LUTを生成
  void createIdentity(int size = 33, LUTType type = LUTType::LUT3D);
  
  // 既存LUTからリサイズ
  void resize(int newSize);
  
  // 2つのLUTを結合
  static LUT combine(const LUT& lut1, const LUT& lut2);
  
  // LUTを反転
  LUT invert() const;

  // ---- LUT適用 ----

  // 単一色に適用
  FloatRGBA apply(const FloatRGBA& color) const;
  FloatRGBA apply(const FloatRGBA& color, const LUTApplyOptions& options) const;
  
  // 画像に適用
  ImageF32x4_RGBA apply(const ImageF32x4_RGBA& image) const;
  ImageF32x4_RGBA apply(const ImageF32x4_RGBA& image, const LUTApplyOptions& options) const;
  
  // 画像に直接適用（in-place）
  void applyInPlace(ImageF32x4_RGBA& image) const;
  void applyInPlace(ImageF32x4_RGBA& image, const LUTApplyOptions& options) const;

  // ---- LUT情報 ----

  // LUT情報を取得
  LUTInfo getInfo() const;
  
  // LUTタイプ
  LUTType getType() const;
  
  // LUTサイズ
  int getSize() const;
  
  // 読み込み済みか
  bool isLoaded() const;
  
  // 恒等変換か
  bool isIdentity() const;

  // ---- ユーティリティ ----

  // サポートされているフォーマット
  static std::vector<LUTFormat> getSupportedFormats();
  
  // フォーマット名を取得
  static UniString getFormatName(LUTFormat format);
  
  // ファイル拡張子からフォーマットを推定
  static LUTFormat detectFormat(const UniString& filePath);
  
  // フォーマットがサポートされているか
  static bool isFormatSupported(LUTFormat format);

  // ---- プリセット ----

  // プリセットLUTを生成
  static LUT createLog2Linear();          // Log → Linear
  static LUT createLinear2Log();          // Linear → Log
  static LUT createSRGB2Linear();         // sRGB → Linear
  static LUT createLinear2SRGB();         // Linear → sRGB
  static LUT createRec709ToRec2020();     // Rec.709 → Rec.2020
  static LUT createContrast(float amount); // コントラスト調整
  static LUT createSaturation(float amount); // 彩度調整

  // ---- デバッグ ----

  // LUTデータをダンプ
  UniString dump() const;
  
  // LUT統計情報
  struct Statistics {
   float minR = 0.0f, maxR = 0.0f;
   float minG = 0.0f, maxG = 0.0f;
   float minB = 0.0f, maxB = 0.0f;
   float avgR = 0.0f, avgG = 0.0f, avgB = 0.0f;
  };
  Statistics getStatistics() const;
 };

 // LUTマネージャー（複数LUTの管理）
 class LIBRARY_DLL_API LUTManager {
 private:
  class Impl;
  Impl* impl_;

 public:
  LUTManager();
  ~LUTManager();

  // LUTを登録
  bool registerLUT(const UniString& name, const LUT& lut);
  
  // LUTを取得
  LUT* getLUT(const UniString& name);
  const LUT* getLUT(const UniString& name) const;
  
  // LUTを削除
  void removeLUT(const UniString& name);
  
  // すべてクリア
  void clear();
  
  // 登録されているLUT名のリスト
  std::vector<UniString> getRegisteredNames() const;
  
  // LUTが登録されているか
  bool hasLUT(const UniString& name) const;
  
  // LUTディレクトリからすべて読み込み
  int loadDirectory(const UniString& directoryPath);
 };

} // namespace ArtifactCore
