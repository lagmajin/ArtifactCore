module;
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wrl/client.h>
#include <QString>
#include <QFile>

module Codec.MFFrameExtractor;

import std;

using Microsoft::WRL::ComPtr;

namespace ArtifactCore {

 class MFFrameExtractor::Impl {
 public:
  bool comInitialized_ = false;
  bool mfInitialized_ = false;
  
  ComPtr<IMFSourceReader> sourceReader_;
  
  // 動画情報
  int64_t duration_ = 0;
  int width_ = 0;
  int height_ = 0;
  double frameRate_ = 0.0;
  QString codecName_;
  
  // 設定
  OutputFormat outputFormat_ = OutputFormat::RGBA;
  int outputWidth_ = 0;
  int outputHeight_ = 0;
  Quality quality_ = Quality::Normal;
  bool hardwareAcceleration_ = true;
  
  // エラー情報
  QString lastError_;
  
  // 統計
  Statistics stats_;
  
  Impl() {
   // COM 初期化
   HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
   comInitialized_ = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;

   // Media Foundation 初期化
   hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
   if (FAILED(hr)) {
    lastError_ = "MFStartup failed";
    return;
   }
   mfInitialized_ = true;
  }
  
  ~Impl() {
   close();
   
   if (mfInitialized_) {
    MFShutdown();
   }
   if (comInitialized_) {
    CoUninitialize();
   }
  }
  
  bool open(const std::wstring& videoPath) {
   close();
   lastError_.clear();
   
   if (!mfInitialized_) {
    lastError_ = "Media Foundation not initialized";
    return false;
   }
   
   // ソースリーダー作成
   ComPtr<IMFAttributes> attributes;
   HRESULT hr = MFCreateAttributes(&attributes, 2);
   if (FAILED(hr)) {
    lastError_ = QString("MFCreateAttributes failed: 0x%1").arg(hr, 0, 16);
    return false;
   }
   
   // ハードウェアアクセラレーション設定
   if (hardwareAcceleration_) {
    attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    attributes->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE);
   }
   
   hr = MFCreateSourceReaderFromURL(videoPath.c_str(), attributes.Get(), &sourceReader_);
   if (FAILED(hr)) {
    lastError_ = QString("Failed to open video file: 0x%1").arg(hr, 0, 16);
    return false;
   }
   
   // ビデオストリーム選択
   hr = sourceReader_->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
   hr = sourceReader_->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
   
   // 出力メディアタイプ設定（RGB32に変換）
   ComPtr<IMFMediaType> outputType;
   hr = MFCreateMediaType(&outputType);
   if (FAILED(hr)) {
    lastError_ = "MFCreateMediaType failed";
    close();
    return false;
   }
   
   outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
   outputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
   
   hr = sourceReader_->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 
                                           nullptr, outputType.Get());
   if (FAILED(hr)) {
    lastError_ = QString("SetCurrentMediaType failed: 0x%1").arg(hr, 0, 16);
    close();
    return false;
   }
   
   // 動画情報を取得
   if (!readVideoInfo()) {
    close();
    return false;
   }
   
   return true;
  }
  
  bool readVideoInfo() {
   ComPtr<IMFMediaType> mediaType;
   HRESULT hr = sourceReader_->GetCurrentMediaType(
    (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &mediaType);
   
   if (FAILED(hr)) {
    lastError_ = "Failed to get media type";
    return false;
   }
   
   // 解像度取得
   UINT32 width, height;
   hr = MFGetAttributeSize(mediaType.Get(), MF_MT_FRAME_SIZE, &width, &height);
   if (SUCCEEDED(hr)) {
    width_ = width;
    height_ = height;
   }
   
   // フレームレート取得
   UINT32 numerator, denominator;
   hr = MFGetAttributeRatio(mediaType.Get(), MF_MT_FRAME_RATE, &numerator, &denominator);
   if (SUCCEEDED(hr) && denominator > 0) {
    frameRate_ = static_cast<double>(numerator) / denominator;
   }
   
   // 動画の長さを取得
   PROPVARIANT var;
   PropVariantInit(&var);
   hr = sourceReader_->GetPresentationAttribute(
    (DWORD)MF_SOURCE_READER_MEDIASOURCE,
    MF_PD_DURATION,
    &var);
   
   if (SUCCEEDED(hr)) {
    duration_ = var.uhVal.QuadPart;
    PropVariantClear(&var);
   }
   
   return true;
  }
  
  void close() {
   sourceReader_.Reset();
   duration_ = 0;
   width_ = 0;
   height_ = 0;
   frameRate_ = 0.0;
   codecName_.clear();
  }
  
  std::unique_ptr<ExtractedFrame> extractFrame(int64_t timestamp) {
   if (!sourceReader_) {
    lastError_ = "Video not opened";
    return nullptr;
   }
   
   auto startTime = std::chrono::high_resolution_clock::now();
   
   // シーク
   PROPVARIANT var;
   PropVariantInit(&var);
   var.vt = VT_I8;
   var.hVal.QuadPart = timestamp;
   
   HRESULT hr = sourceReader_->SetCurrentPosition(GUID_NULL, var);
   PropVariantClear(&var);
   
   if (FAILED(hr)) {
    lastError_ = QString("Seek failed: 0x%1").arg(hr, 0, 16);
    return nullptr;
   }
   
   // サンプル読み取り
   ComPtr<IMFSample> sample;
   DWORD streamFlags = 0;
   LONGLONG sampleTimestamp = 0;
   
   hr = sourceReader_->ReadSample(
    (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
    0,
    nullptr,
    &streamFlags,
    &sampleTimestamp,
    &sample);
   
   if (FAILED(hr)) {
    lastError_ = QString("ReadSample failed: 0x%1").arg(hr, 0, 16);
    return nullptr;
   }
   
   if (streamFlags & MF_SOURCE_READERF_ENDOFSTREAM) {
    lastError_ = "End of stream";
    return nullptr;
   }
   
   if (!sample) {
    lastError_ = "No sample returned";
    return nullptr;
   }
   
   // メディアバッファーを取得
   ComPtr<IMFMediaBuffer> buffer;
   hr = sample->ConvertToContiguousBuffer(&buffer);
   if (FAILED(hr)) {
    lastError_ = "ConvertToContiguousBuffer failed";
    return nullptr;
   }
   
   // データをロック
   BYTE* data = nullptr;
   DWORD maxLength = 0, currentLength = 0;
   hr = buffer->Lock(&data, &maxLength, &currentLength);
   if (FAILED(hr)) {
    lastError_ = "Buffer Lock failed";
    return nullptr;
   }
   
   // ExtractedFrame を作成
   auto frame = std::make_unique<ExtractedFrame>();
   
   int outputWidth = (outputWidth_ > 0) ? outputWidth_ : width_;
   int outputHeight = (outputHeight_ > 0) ? outputHeight_ : height_;
   int stride = outputWidth * 4;  // RGBA
   
   frame->width = outputWidth;
   frame->height = outputHeight;
   frame->stride = stride;
   frame->timestamp = sampleTimestamp;
   frame->frameNumber = static_cast<int>(sampleTimestamp * frameRate_ / 10000000);
   frame->data.resize(stride * outputHeight);
   
   // データをコピー（RGB32 → RGBA）
   if (outputWidth == width_ && outputHeight == height_) {
    // リサイズ不要
    std::memcpy(frame->data.data(), data, currentLength);
   } else {
    // 簡易リサイズ（実際にはより高品質なリサイズアルゴリズムを使用すべき）
    // TODO: より高品質なリサイズ実装
    std::memcpy(frame->data.data(), data, std::min<size_t>(currentLength, frame->data.size()));
   }
   
   // フォーマット変換
   if (outputFormat_ != OutputFormat::RGBA) {
    convertFormat(frame->data, frame->width, frame->height, outputFormat_);
   }
   
   buffer->Unlock();
   
   // 統計更新
   auto endTime = std::chrono::high_resolution_clock::now();
   auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
   
   stats_.totalFramesExtracted++;
   stats_.totalBytesProcessed += frame->data.size();
   stats_.averageExtractionTimeMs = 
    (stats_.averageExtractionTimeMs * (stats_.totalFramesExtracted - 1) + duration.count()) 
    / stats_.totalFramesExtracted;
   
   return frame;
  }
  
  void convertFormat(std::vector<uint8_t>& data, int width, int height, OutputFormat format) {
   // RGBA → 他のフォーマットへの変換
   // 実装は format に応じて変更
   // 簡易実装（完全版は各フォーマットに対応）
   
   switch (format) {
   case OutputFormat::BGR:
    // RGBA → BGR（アルファチャンネル削除 & R/B入れ替え）
    {
     std::vector<uint8_t> temp;
     temp.reserve(width * height * 3);
     for (size_t i = 0; i < data.size(); i += 4) {
      temp.push_back(data[i + 2]);  // B
      temp.push_back(data[i + 1]);  // G
      temp.push_back(data[i + 0]);  // R
     }
     data = std::move(temp);
    }
    break;
    
   case OutputFormat::RGB:
    // RGBA → RGB（アルファチャンネル削除）
    {
     std::vector<uint8_t> temp;
     temp.reserve(width * height * 3);
     for (size_t i = 0; i < data.size(); i += 4) {
      temp.push_back(data[i + 0]);  // R
      temp.push_back(data[i + 1]);  // G
      temp.push_back(data[i + 2]);  // B
     }
     data = std::move(temp);
    }
    break;
    
   case OutputFormat::BGRA:
    // RGBA → BGRA（R/B入れ替え）
    for (size_t i = 0; i < data.size(); i += 4) {
     std::swap(data[i], data[i + 2]);
    }
    break;
    
   default:
    break;
   }
  }
 };

 // MFFrameExtractor 実装
 
 MFFrameExtractor::MFFrameExtractor() : impl_(std::make_unique<Impl>()) {}

 MFFrameExtractor::~MFFrameExtractor() = default;

 bool MFFrameExtractor::open(const QString& videoPath) {
  return open(videoPath.toStdWString());
 }

 bool MFFrameExtractor::open(const std::wstring& videoPath) {
  return impl_->open(videoPath);
 }

 void MFFrameExtractor::close() {
  impl_->close();
 }

 bool MFFrameExtractor::isOpen() const {
  return impl_->sourceReader_ != nullptr;
 }

 int64_t MFFrameExtractor::getDuration() const {
  return impl_->duration_;
 }

 double MFFrameExtractor::getDurationSeconds() const {
  return impl_->duration_ / 10000000.0;
 }

 int MFFrameExtractor::getWidth() const {
  return impl_->width_;
 }

 int MFFrameExtractor::getHeight() const {
  return impl_->height_;
 }

 double MFFrameExtractor::getFrameRate() const {
  return impl_->frameRate_;
 }

 int64_t MFFrameExtractor::getTotalFrames() const {
  if (impl_->frameRate_ <= 0.0) return 0;
  return static_cast<int64_t>(getDurationSeconds() * impl_->frameRate_);
 }

 QString MFFrameExtractor::getCodecName() const {
  return impl_->codecName_;
 }

 std::unique_ptr<ExtractedFrame> MFFrameExtractor::extractFrameAtTime(int64_t timestamp) {
  return impl_->extractFrame(timestamp);
 }

 std::unique_ptr<ExtractedFrame> MFFrameExtractor::extractFrameAtIndex(int64_t frameIndex) {
  if (impl_->frameRate_ <= 0.0) return nullptr;
  int64_t timestamp = static_cast<int64_t>(frameIndex * 10000000.0 / impl_->frameRate_);
  return extractFrameAtTime(timestamp);
 }

 std::unique_ptr<ExtractedFrame> MFFrameExtractor::extractFrameAtSeconds(double seconds) {
  int64_t timestamp = static_cast<int64_t>(seconds * 10000000.0);
  return extractFrameAtTime(timestamp);
 }

 std::vector<std::unique_ptr<ExtractedFrame>> MFFrameExtractor::extractFrames(
  const std::vector<int64_t>& frameIndices) {
  std::vector<std::unique_ptr<ExtractedFrame>> frames;
  frames.reserve(frameIndices.size());
  
  for (int64_t index : frameIndices) {
   auto frame = extractFrameAtIndex(index);
   if (frame) {
    frames.push_back(std::move(frame));
   }
  }
  
  return frames;
 }

 std::vector<std::unique_ptr<ExtractedFrame>> MFFrameExtractor::extractUniformFrames(int count) {
  int64_t totalFrames = getTotalFrames();
  if (totalFrames <= 0 || count <= 0) return {};
  
  std::vector<int64_t> indices;
  indices.reserve(count);
  
  for (int i = 0; i < count; ++i) {
   int64_t frameIndex = (totalFrames * i) / count;
   indices.push_back(frameIndex);
  }
  
  return extractFrames(indices);
 }

 std::vector<std::unique_ptr<ExtractedFrame>> MFFrameExtractor::extractFrameRange(
  int64_t startFrame, int64_t endFrame, int step) {
  std::vector<int64_t> indices;
  for (int64_t i = startFrame; i <= endFrame; i += step) {
   indices.push_back(i);
  }
  return extractFrames(indices);
 }

 void MFFrameExtractor::setOutputFormat(OutputFormat format) {
  impl_->outputFormat_ = format;
 }

 MFFrameExtractor::OutputFormat MFFrameExtractor::getOutputFormat() const {
  return impl_->outputFormat_;
 }

 void MFFrameExtractor::setOutputSize(int width, int height) {
  impl_->outputWidth_ = width;
  impl_->outputHeight_ = height;
 }

 void MFFrameExtractor::clearOutputSize() {
  impl_->outputWidth_ = 0;
  impl_->outputHeight_ = 0;
 }

 bool MFFrameExtractor::hasCustomOutputSize() const {
  return impl_->outputWidth_ > 0 && impl_->outputHeight_ > 0;
 }

 void MFFrameExtractor::setQuality(Quality quality) {
  impl_->quality_ = quality;
 }

 MFFrameExtractor::Quality MFFrameExtractor::getQuality() const {
  return impl_->quality_;
 }

 void MFFrameExtractor::setHardwareAcceleration(bool enable) {
  impl_->hardwareAcceleration_ = enable;
 }

 bool MFFrameExtractor::isHardwareAccelerationEnabled() const {
  return impl_->hardwareAcceleration_;
 }

 QString MFFrameExtractor::lastError() const {
  return impl_->lastError_;
 }

 bool MFFrameExtractor::hasError() const {
  return !impl_->lastError_.isEmpty();
 }

 void MFFrameExtractor::clearError() {
  impl_->lastError_.clear();
 }

 MFFrameExtractor::Statistics MFFrameExtractor::getStatistics() const {
  return impl_->stats_;
 }

 void MFFrameExtractor::resetStatistics() {
  impl_->stats_ = Statistics{};
 }

 // MfEncoderEnumerator 実装（後方互換性）
 
 std::vector<MfEncoderEnumerator::EncoderInfo> MfEncoderEnumerator::ListAvailableVideoEncoders(GUID subtype) {
  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
  if (FAILED(hr)) {
   return {};
  }

  std::vector<EncoderInfo> list;

  IMFActivate** ppActivate = nullptr;
  UINT32 count = 0;

  MFT_REGISTER_TYPE_INFO info = {};
  info.guidMajorType = MFMediaType_Video;
  info.guidSubtype = subtype;

  hr = MFTEnumEx(
   MFT_CATEGORY_VIDEO_ENCODER,
   MFT_ENUM_FLAG_SYNCMFT,
   &info,
   nullptr,
   &ppActivate,
   &count);

  if (SUCCEEDED(hr)) {
   for (UINT32 i = 0; i < count; ++i) {
    WCHAR* szName = nullptr;
    UINT32 cchName = 0;
    if (SUCCEEDED(ppActivate[i]->GetAllocatedString(
     MFT_FRIENDLY_NAME_Attribute, &szName, &cchName))) {
     EncoderInfo info{ szName, GUID_NULL };
     ppActivate[i]->GetGUID(MFT_TRANSFORM_CLSID_Attribute, &info.clsid);
     list.push_back(info);
     CoTaskMemFree(szName);
    }
    ppActivate[i]->Release();
   }
   CoTaskMemFree(ppActivate);
  }
  
  MFShutdown();
  CoUninitialize();

  return list;
 }

}
