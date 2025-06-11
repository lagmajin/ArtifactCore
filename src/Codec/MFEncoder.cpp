module;
#include <mfapi.h>
#include <mfidl.h>
module Codec:MFEncoder;

import std;

namespace ArtifactCore {




 MFEncoder::MFEncoder()
 {
  // COM èâä˙âª
  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  com_initialized = SUCCEEDED(hr);

  // Media Foundation èâä˙âª
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

}