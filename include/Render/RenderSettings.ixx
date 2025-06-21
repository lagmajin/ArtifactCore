module;
#include <QString>


export module Render:RenderSettings;


export void MyFunc();



export namespace ArtifactCore
{

 enum class OutputMode {
  Video,
  ImageSequence,
  AudioOnly
 };


 struct RenderSettings {
  OutputMode mode = OutputMode::Video; // ���C���o�̓��[�h
  bool exportAudioSeparately = false;  // ������ʏo�͂��邩
  QString inputPath;
  QString outputVideoPath;
  QString outputAudioPath;             // exportAudioSeparately=true �̂Ƃ��L��

 };










}