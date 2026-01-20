module;
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <memory>
#include <QString>
#include "../Define/DllExportMacro.hpp"

export module Codec.MFFrameExtractor;

import std;

export namespace ArtifactCore {

 // フレーム抽出結果
 struct ExtractedFrame {
  std::vector<uint8_t> data;  // RGBA または BGR データ
  int width = 0;
  int height = 0;
  int stride = 0;
  int64_t timestamp = 0;  // 100ナノ秒単位
  int frameNumber = 0;
  
  // 検証
  bool isValid() const { return !data.empty() && width > 0 && height > 0; }
  
  // バイトサイズ
  size_t dataSize() const { return data.size(); }
 };

 // 動画からフレーム（画像）を抽出するクラス
 class LIBRARY_DLL_API MFFrameExtractor {
 public:
  MFFrameExtractor();
  ~MFFrameExtractor();

  // コピー/ムーブ禁止（COMリソース管理のため）
  MFFrameExtractor(const MFFrameExtractor&) = delete;
  MFFrameExtractor& operator=(const MFFrameExtractor&) = delete;
  MFFrameExtractor(MFFrameExtractor&&) = delete;
  MFFrameExtractor& operator=(MFFrameExtractor&&) = delete;

  // 動画ファイルを開く
  bool open(const QString& videoPath);
  bool open(const std::wstring& videoPath);
  
  // ファイルを閉じる
  void close();

  // 動画情報の取得
  bool isOpen() const;
  int64_t getDuration() const;  // 100ナノ秒単位
  double getDurationSeconds() const;
  int getWidth() const;
  int getHeight() const;
  double getFrameRate() const;
  int64_t getTotalFrames() const;
  QString getCodecName() const;

  // フレーム抽出
  // 特定の時刻のフレームを取得（100ナノ秒単位）
  std::unique_ptr<ExtractedFrame> extractFrameAtTime(int64_t timestamp);
  
  // 特定のフレーム番号のフレームを取得
  std::unique_ptr<ExtractedFrame> extractFrameAtIndex(int64_t frameIndex);
  
  // 特定の秒数のフレームを取得
  std::unique_ptr<ExtractedFrame> extractFrameAtSeconds(double seconds);
  
  // 複数フレームを一度に抽出（サムネイル生成用）
  std::vector<std::unique_ptr<ExtractedFrame>> extractFrames(
   const std::vector<int64_t>& frameIndices);
  
  // 均等間隔でフレームを抽出（プレビュー用）
  std::vector<std::unique_ptr<ExtractedFrame>> extractUniformFrames(int count);
  
  // 範囲内のフレームを抽出（アニメーション用）
  std::vector<std::unique_ptr<ExtractedFrame>> extractFrameRange(
   int64_t startFrame, int64_t endFrame, int step = 1);
  
  // 出力フォーマット設定
  enum class OutputFormat {
   RGBA,      // 32bit RGBA
   RGB,       // 24bit RGB
   BGR,       // 24bit BGR (OpenCV互換)
   BGRA       // 32bit BGRA
  };
  
  void setOutputFormat(OutputFormat format);
  OutputFormat getOutputFormat() const;
  
  // リサイズ設定（出力時に自動リサイズ）
  void setOutputSize(int width, int height);
  void clearOutputSize();  // オリジナルサイズを使用
  bool hasCustomOutputSize() const;
  
  // 品質設定
  enum class Quality {
   Draft,     // 低品質・高速
   Normal,    // 通常品質
   High       // 高品質・低速
  };
  
  void setQuality(Quality quality);
  Quality getQuality() const;
  
  // デコーダー設定
  void setHardwareAcceleration(bool enable);
  bool isHardwareAccelerationEnabled() const;
  
  // エラー情報
  QString lastError() const;
  bool hasError() const;
  void clearError();

  // 統計情報
  struct Statistics {
   int64_t totalFramesExtracted = 0;
   int64_t totalBytesProcessed = 0;
   double averageExtractionTimeMs = 0.0;
  };
  
  Statistics getStatistics() const;
  void resetStatistics();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
 };

 // エンコーダー列挙（後方互換性のため残す）
 class LIBRARY_DLL_API MfEncoderEnumerator {
 public:
  struct EncoderInfo {
   std::wstring friendlyName;
   GUID clsid;
  };

  static std::vector<EncoderInfo> ListAvailableVideoEncoders(GUID subtype);
 };

 // 旧クラス名のエイリアス（後方互換性）
 using MFEncoder = MFFrameExtractor;
}
