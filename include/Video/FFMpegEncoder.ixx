module;

#include <QFile>

export module Encoder.FFmpegEncoder;

//import EncoderSettings;

import Image;


export namespace ArtifactCore {

 

 //ffmpeg encoder
 class FFmpegEncoder {
 private:
  class Impl;
  Impl* impl_;
 public:
  FFmpegEncoder();
  ~FFmpegEncoder();

  FFmpegEncoder(const FFmpegEncoder&) = delete;
  FFmpegEncoder& operator=(const FFmpegEncoder&) = delete;

  //open file
  void open(const QFile& file);
  void addImage(const ImageF32x4_RGBA& image);
  void close();
 };



}



