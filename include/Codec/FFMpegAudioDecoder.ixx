module ;
#include "../Define/DllExportMacro.hpp"
#include <QList>
#include <QString>


export module Media.Encoder.FFMpegAudioDecoder;

import std;
import Utils.Size.Like;
import Utils.String.UniString;

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
  void fillCacheAsync(double start, double end);
  void flush();
  bool isSameFile(const UniString& path) const;
 };





};