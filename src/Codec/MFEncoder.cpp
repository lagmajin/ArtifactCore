module ;
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>


module Codec.MFEncoder;
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
import std;

namespace ArtifactCore {




 MFEncoder::MFEncoder()
 {
  // COM 初期化
  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  com_initialized = SUCCEEDED(hr);

  // Media Foundation 初期化
  hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
  if (FAILED(hr)) {
   throw std::runtime_error("MFStartup failed");
  }
  mf_initialized = true;
 }

 MFEncoder::~MFEncoder()
 {
  if (mf_initialized) {
   MFShutdown();
  }
  if (com_initialized) {
   CoUninitialize();
  }
 }

 void MFEncoder::InitWriter(const std::wstring& outputPath)
 {
  Microsoft::WRL::ComPtr<IMFAttributes> attributes;
  HRESULT hr = MFCreateAttributes(&attributes, 1);
  if (FAILED(hr)) throw std::runtime_error("MFCreateAttributes failed");

  hr = MFCreateSinkWriterFromURL(outputPath.c_str(), nullptr, attributes.Get(), &sinkWriter);
  if (FAILED(hr)) throw std::runtime_error("MFCreateSinkWriterFromURL failed");

  // 実際の MediaType 設定やストリーム追加はここで行う
 }

 void MFEncoder::close()
 {

 }

 std::vector<ArtifactCore::MfEncoderEnumerator::EncoderInfo> MfEncoderEnumerator::ListAvailableVideoEncoders(GUID subtype)
 {
  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  //com_initialized = SUCCEEDED(hr);

  // Media Foundation 初期化
  hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
  if (FAILED(hr)) {
   throw std::runtime_error("MFStartup failed");
  }
  //mf_initialized = true;

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
   &count
  );

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

  return list;
 }

}