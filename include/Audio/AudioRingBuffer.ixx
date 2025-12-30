module;
#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QVector>
#include "../Define/DllExportMacro.hpp"
export module Audio.RingBuffer;

export namespace ArtifactCore {
 class AudioRingBuffer {
 private:
  class Impl;
  Impl* impl_;
 public:
  AudioRingBuffer();
  ~AudioRingBuffer();
 };



};