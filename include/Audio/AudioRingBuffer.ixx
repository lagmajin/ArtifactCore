module;
#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QVector>
#include "../Define/DllExportMacro.hpp"
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
export module Audio.RingBuffer;




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