module;
class tst_QList;
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>

export module Audio.BufferQueue;

import Audio.Segment;

export namespace ArtifactCore {

 class AudioBufferQueue {
 private:
  QList<AudioSegment> queue; 
  mutable QMutex mutex;
  QWaitCondition notEmpty;
  const int maxSegments = 100;
 public:
  AudioBufferQueue() = default;
  ~AudioBufferQueue() = default;

  bool push(const AudioSegment& segment) {
   const int frames = segment.frameCount();
   const int channels = segment.channelCount();
   if (frames <= 0 || channels <= 0 || segment.sampleRate <= 0) {
    return false;
   }
   for (const auto& channel : segment.channelData) {
    if (channel.size() < frames) {
     return false;
    }
   }
   QMutexLocker locker(&mutex);
   if (queue.size() >= maxSegments) {
    return false;
   }
   queue.append(segment); 
   notEmpty.wakeOne();
   return true;
  }

  bool pop(AudioSegment& outSegment) {
   QMutexLocker locker(&mutex);
   if (queue.isEmpty()) return false;
   outSegment = queue.takeFirst();
   return true;
  }

  bool isFull() const {
   QMutexLocker locker(&mutex);
   return queue.size() >= maxSegments;
  }

  bool isEmpty() const {
   QMutexLocker locker(&mutex);
   return queue.isEmpty();
  }

  void clear() {
   QMutexLocker locker(&mutex);
   queue.clear();
  }

 };

};
