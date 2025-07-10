

module;
#include "../Define/DllExportMacro.hpp"

#include <QString>
#include <QImage>
#include <QByteArray>
#include <libavformat/avformat.h>

export module Codec.FFMpegDecoder;

//struct AVFrame;
export namespace ArtifactCore {
 enum class MediaType {
  Video,
  Audio,
  EndOfFile, // �X�g���[���̏I�[
  None       // ���������Ȃ������ꍇ
 };

 struct MediaFrame {
  MediaType type = MediaType::None;
  double    timestamp = 0.0; // �^�C���X�^���v�i�b�j

  // �r�f�I�t���[���̏ꍇ
  QImage    videoImage;

  // �I�[�f�B�I�t���[���̏ꍇ
  QByteArray audioSamples; // ���̃I�[�f�B�I�f�[�^ (��: S16�`��)
  int       audioChannels = 0;
  int       audioSampleRate = 0;
  // �K�v�ɉ����āA�I�[�f�B�I�̃T���v���t�H�[�}�b�g (AV_SAMPLE_FMT_S16�Ȃ�) ���ǉ�
 };

 

 class LIBRARY_DLL_API FFMpegDecoder {
 private:
  class Impl;
  Impl* impl_;
 public:
  FFMpegDecoder() noexcept;

  ~FFMpegDecoder();
  bool openFile(const QString& path);

  void closeFile();

  AVFrame* decodeNextVideoFrame();

 };







};