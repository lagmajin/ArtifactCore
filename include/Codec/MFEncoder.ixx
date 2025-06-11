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

  // �G���R�[�h�p�̌��J���\�b�h�i��: �������⏑�����݁j
  void InitWriter(const std::wstring& outputPath) {
   Microsoft::WRL::ComPtr<IMFAttributes> attributes;
   HRESULT hr = MFCreateAttributes(&attributes, 1);
   if (FAILED(hr)) throw std::runtime_error("MFCreateAttributes failed");

   hr = MFCreateSinkWriterFromURL(outputPath.c_str(), nullptr, attributes.Get(), &sinkWriter);
   if (FAILED(hr)) throw std::runtime_error("MFCreateSinkWriterFromURL failed");

   // ���ۂ� MediaType �ݒ��X�g���[���ǉ��͂����ōs��
  }

  // ���̃G���R�[�h���상�\�b�h�������ɒǉ�
  // ��: AddVideoStream(), WriteFrame(), Finalize() �Ȃ�

 private:
  bool com_initialized = false;
  bool mf_initialized = false;

  Microsoft::WRL::ComPtr<IMFSinkWriter> sinkWriter;
 };

}