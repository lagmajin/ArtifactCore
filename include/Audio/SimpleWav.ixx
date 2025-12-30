module;
#include <QVector>
#include <QByteArray>
#include <QtMultimedia/QAudioFormat>
#include "../Define/DllExportMacro.hpp"


export module Audio.SimpleWav;

import Utils.String.UniString;

export namespace ArtifactCore {
 
 class LIBRARY_DLL_API SimpleWav
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  SimpleWav();
  ~SimpleWav();
  bool loadFromFile(const QString& filePath);
  bool loadFromFile(const UniString& filepath);
  int sampleRate() const;
  int bitDepth() const;
  int channelCount() const;
  qint64 frameCount() const;
  QVector<float> getAudioData() const;
 };

};