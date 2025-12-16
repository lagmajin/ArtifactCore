
module;
#include <QString>

#include "../Define/DllExportMacro.hpp"
export module Media.Encoder.FFMpegAudioDecoder;

import std;
import Utils.Size.Like;


export namespace ArtifactCore
{

 class LIBRARY_DLL_API FFMpegAudioDecoder
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  FFMpegAudioDecoder();
  ~FFMpegAudioDecoder();
  bool openFile(const QString& path);
  void closeFile();
  void seek(double seek);
 };





};