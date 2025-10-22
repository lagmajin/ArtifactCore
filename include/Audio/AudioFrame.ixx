module;
extern "C" {
#include <libavutil\samplefmt.h>
}
#include <QByteArray>
export module Audio.Frame;


import std;

export namespace ArtifactCore {

 struct AudioFrame {
  QByteArray pcmData;       // PCMデータ（16bit, 32bitなど）
  int sampleRate = 0;       // サンプリングレート（例：44100Hz）
  int channels = 0;         // チャンネル数（例：2）
  AVSampleFormat format = AV_SAMPLE_FMT_NONE; // FFmpeg形式の保持も可能（変換に便利）
  qint64 pts = 0;           // presentation timestamp（ミリ秒 or AVStreamのtime_base基準）

  bool isValid() const {
   return !pcmData.isEmpty() && sampleRate > 0 && channels > 0;
  }
 };





};