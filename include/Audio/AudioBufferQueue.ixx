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

  void push(const AudioSegment& segment) {
   QMutexLocker locker(&mutex);
   queue.append(segment); 
   notEmpty.wakeOne();
  }

  bool pop(AudioSegment& outSegment) {
   QMutexLocker locker(&mutex);
   if (queue.isEmpty()) return false;
   outSegment = queue.takeFirst();
   return true;
  }

  void clear() {
   QMutexLocker locker(&mutex);
   queue.clear();
  }

 };

};
