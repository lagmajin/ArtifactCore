module;
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <string>

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
export module Codec.MFEncoder;

#include <QString>





export namespace ArtifactCore {

 class MFEncoder {
 public:
  MFEncoder();

  ~MFEncoder();

  // GR[hp̌J\bhi: ⏑݁j
  void InitWriter(const std::wstring& outputPath);
  void close();
  // ̃GR[h상\bhɒǉ
  // : AddVideoStream(), WriteFrame(), Finalize() Ȃ

 private:
  bool com_initialized = false;
  bool mf_initialized = false;

  Microsoft::WRL::ComPtr<IMFSinkWriter> sinkWriter;
 };


 class LIBRARY_DLL_API MfEncoderEnumerator {
 public:
  struct EncoderInfo {
   std::wstring friendlyName;
   GUID clsid;
  };

  static std::vector<EncoderInfo> ListAvailableVideoEncoders(GUID subtype);
 };
}
