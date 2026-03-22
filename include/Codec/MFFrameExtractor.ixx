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
export module Codec.MFFrameExtractor;





export namespace ArtifactCore {

 // t[o
 struct ExtractedFrame {
  std::vector<uint8_t> data;  // RGBA ܂ BGR f[^
  int width = 0;
  int height = 0;
  int stride = 0;
  int64_t timestamp = 0;  // 100imbP
  int frameNumber = 0;
  
  // 
  bool isValid() const { return !data.empty() && width > 0 && height > 0; }
  
  // oCgTCY
  size_t dataSize() const { return data.size(); }
 };

 // 悩t[i摜j𒊏oNX
 class LIBRARY_DLL_API MFFrameExtractor {
 public:
  MFFrameExtractor();
  ~MFFrameExtractor();

  // Rs[/[u֎~iCOM\[XǗ̂߁j
  MFFrameExtractor(const MFFrameExtractor&) = delete;
  MFFrameExtractor& operator=(const MFFrameExtractor&) = delete;
  MFFrameExtractor(MFFrameExtractor&&) = delete;
  MFFrameExtractor& operator=(MFFrameExtractor&&) = delete;

  // t@CJ
  bool open(const QString& videoPath);
  bool open(const std::wstring& videoPath);
  
  // t@C
  void close();

  // ̎擾
  bool isOpen() const;
  int64_t getDuration() const;  // 100imbP
  double getDurationSeconds() const;
  int getWidth() const;
  int getHeight() const;
  double getFrameRate() const;
  int64_t getTotalFrames() const;
  QString getCodecName() const;

  // t[o
  // ̎̃t[擾i100imbPʁj
  std::unique_ptr<ExtractedFrame> extractFrameAtTime(int64_t timestamp);
  
  // ̃t[ԍ̃t[擾
  std::unique_ptr<ExtractedFrame> extractFrameAtIndex(int64_t frameIndex);
  
  // ̕b̃t[擾
  std::unique_ptr<ExtractedFrame> extractFrameAtSeconds(double seconds);
  
  // t[xɒoiTlCpj
  std::vector<std::unique_ptr<ExtractedFrame>> extractFrames(
   const std::vector<int64_t>& frameIndices);
  
  // ϓԊuŃt[𒊏oivr[pj
  std::vector<std::unique_ptr<ExtractedFrame>> extractUniformFrames(int count);
  
  // ͈͓̃t[𒊏oiAj[Vpj
  std::vector<std::unique_ptr<ExtractedFrame>> extractFrameRange(
   int64_t startFrame, int64_t endFrame, int step = 1);
  
  // o̓tH[}bgݒ
  enum class OutputFormat {
   RGBA,      // 32bit RGBA
   RGB,       // 24bit RGB
   BGR,       // 24bit BGR (OpenCV݊)
   BGRA       // 32bit BGRA
  };
  
  void setOutputFormat(OutputFormat format);
  OutputFormat getOutputFormat() const;
  
  // TCYݒio͎ɎTCYj
  void setOutputSize(int width, int height);
  void clearOutputSize();  // IWiTCYgp
  bool hasCustomOutputSize() const;
  
  // iݒ
  enum class Quality {
   Draft,     // iE
   Normal,    // ʏi
   High       // iEᑬ
  };
  
  void setQuality(Quality quality);
  Quality getQuality() const;
  
  // fR[_[ݒ
  void setHardwareAcceleration(bool enable);
  bool isHardwareAccelerationEnabled() const;
  
  // G[
  QString lastError() const;
  bool hasError() const;
  void clearError();

  // v
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

 // GR[_[񋓁i݊̂ߎcj
 class LIBRARY_DLL_API MfEncoderEnumerator {
 public:
  struct EncoderInfo {
   std::wstring friendlyName;
   GUID clsid;
  };

  static std::vector<EncoderInfo> ListAvailableVideoEncoders(GUID subtype);
 };

 // NX̃GCAXi݊j
 using MFEncoder = MFFrameExtractor;
}
