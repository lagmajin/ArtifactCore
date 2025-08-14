module;
#include <QString>


export module Render.Settings;

import Log;



export namespace ArtifactCore
{

 enum class OutputMode {
  Video,
  ImageSequence,
  AudioOnly
 };


 struct RenderProcessSettings {
  bool useMultiCore = true;
  bool multiFrameRendering = true;
  int maxThreads = 0; // 0で自動
  bool enableGPUAcceleration = true;
  // ... その他、ロギングレベル、エラー継続など
 };


 struct RenderAudioSettings {
  int sampleRate = 48000;
  int bitDepth = 24;
  int channels = 2;
  QString codec = "WAV";
  bool enableAudio = true;
  bool exportSeparately = false;
  QString outputPath;
 };

 struct RenderSettings {
  OutputMode mode = OutputMode::Video; // メイン出力モード
  bool exportAudioSeparately = false;  // 音声を別出力するか
  QString inputPath;
  QString outputVideoPath;
  QString outputAudioPath;             // exportAudioSeparately=true のとき有効

 };


 






}