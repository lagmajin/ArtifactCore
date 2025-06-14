module;
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <string>

#include <QString>

export module Codec:MFEncoder;

import std;

export namespace ArtifactCore {

 class MFEncoder {
 public:
  MFEncoder();

  ~MFEncoder();

  // エンコード用の公開メソッド（例: 初期化や書き込み）
  void InitWriter(const std::wstring& outputPath);
  void close();
  // 他のエンコード操作メソッドをここに追加
  // 例: AddVideoStream(), WriteFrame(), Finalize() など

 private:
  bool com_initialized = false;
  bool mf_initialized = false;

  Microsoft::WRL::ComPtr<IMFSinkWriter> sinkWriter;
 };


 class MfEncoderEnumerator {
 public:
  struct EncoderInfo {
   std::wstring friendlyName;
   GUID clsid;
  };

  static std::vector<EncoderInfo> ListAvailableVideoEncoders(GUID subtype);
 };
}