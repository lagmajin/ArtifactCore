module;
#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QVector>
#include "../Define/DllExportMacro.hpp"
export module Audio.RingBuffer;

import std;
import Audio.Segment;

export namespace ArtifactCore {
 class AudioRingBuffer {
 private:
  class Impl;
  Impl* impl_;
 public:
  AudioRingBuffer();
  AudioRingBuffer(size_t capacity);
  ~AudioRingBuffer();

  // Capacity management
  void setCapacity(size_t capacity);
  size_t capacity() const;

  // Data operations
  bool write(const AudioSegment& data);
  bool read(AudioSegment& data, size_t size);
  size_t available() const;
  size_t freeSpace() const;

  // Utility
  void clear();
  bool isEmpty() const;
  bool isFull() const;
 };

};