module;
#include <QList>
#include <QMutex>
#include <QWaitCondition>
export module Audio.BufferQueue;

import std;
import  Audio.Segment;

export namespace ArtifactCore {



 class AudioBufferQueue {
 private:
  QList<AudioSegment> queue; // AudioSegmentのリスト
  mutable QMutex mutex;
  QWaitCondition notEmpty;
  const int maxSegments = 100;
 public:
  AudioBufferQueue();
  ~AudioBufferQueue();

  void push(const AudioSegment& segment) {
   QMutexLocker locker(&mutex);
   queue.append(segment); // QVectorの共有により高速
   notEmpty.wakeOne();
  }

  // 再生スレッドが呼ぶ
  bool pop(AudioSegment& outSegment) {
   QMutexLocker locker(&mutex);
   if (queue.isEmpty()) return false;
   outSegment = queue.takeFirst();
   return true;
  }

  void clear() {
   QMutexLocker locker(&mutex);
   queue.clear(); // シーク時に必須
  }

 };

};