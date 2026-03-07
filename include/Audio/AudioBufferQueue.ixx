module;
#include <QList>
#include <QMutex>
#include <QWaitCondition>
export module Audio.BufferQueue;

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>



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