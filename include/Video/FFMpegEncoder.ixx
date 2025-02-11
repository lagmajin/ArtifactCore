module;

#include <QtCore/QFile>

export module FFMpegEncoder;

import EncoderSettings;


export namespace ArtifactCore {

 class FFMpegEncoderPrivate;

 //ffmpeg encoder
 class FFMpegEncoder {
 private:

 public:
  FFMpegEncoder();
  ~FFMpegEncoder();
  //open file
  void open(const QFile& file);
  void close();
 };



}



