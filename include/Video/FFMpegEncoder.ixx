module;

#include <QtCore/QFile>

export module Encoder:FFMpegEncoder;

//import EncoderSettings;

import Image;


export namespace ArtifactCore {

 class FFMpegEncoderPrivate;

 //ffmpeg encoder
 class FFMpegEncoder {
 private:
  class Impl;
  std::unique_ptr<Impl> impl;
 public:
  FFMpegEncoder();
  ~FFMpegEncoder();
  //open file
  void open(const QFile& file);
  void addImage(const ImageF32x4_RGBA& image);
  void close();
 };



}



