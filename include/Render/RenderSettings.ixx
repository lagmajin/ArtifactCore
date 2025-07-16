module;
#include <QString>


export module Render:RenderSettings;





export namespace ArtifactCore
{

 enum class OutputMode {
  Video,
  ImageSequence,
  AudioOnly
 };


 struct RenderSettings {
  OutputMode mode = OutputMode::Video; // メイン出力モード
  bool exportAudioSeparately = false;  // 音声を別出力するか
  QString inputPath;
  QString outputVideoPath;
  QString outputAudioPath;             // exportAudioSeparately=true のとき有効

 };










}