module;
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <string>

export module Codec:MFEncoder;

import std;

export namespace ArtifactCore {

 class MFEncoder {
 public:
  MFEncoder();

  ~MFEncoder();

  // エンコード用の公開メソッド（例: 初期化や書き込み）
  void InitWriter(const std::wstring& outputPath) {
   Microsoft::WRL::ComPtr<IMFAttributes> attributes;
   HRESULT hr = MFCreateAttributes(&attributes, 1);
   if (FAILED(hr)) throw std::runtime_error("MFCreateAttributes failed");

   hr = MFCreateSinkWriterFromURL(outputPath.c_str(), nullptr, attributes.Get(), &sinkWriter);
   if (FAILED(hr)) throw std::runtime_error("MFCreateSinkWriterFromURL failed");

   // 実際の MediaType 設定やストリーム追加はここで行う
  }

  // 他のエンコード操作メソッドをここに追加
  // 例: AddVideoStream(), WriteFrame(), Finalize() など

 private:
  bool com_initialized = false;
  bool mf_initialized = false;

  Microsoft::WRL::ComPtr<IMFSinkWriter> sinkWriter;
 };

}