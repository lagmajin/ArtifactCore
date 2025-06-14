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

  // �G���R�[�h�p�̌��J���\�b�h�i��: �������⏑�����݁j
  void InitWriter(const std::wstring& outputPath);
  void close();
  // ���̃G���R�[�h���상�\�b�h�������ɒǉ�
  // ��: AddVideoStream(), WriteFrame(), Finalize() �Ȃ�

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