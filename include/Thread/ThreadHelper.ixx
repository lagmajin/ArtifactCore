module;
#include <utility>

//import <QString>;
#include <type_traits>
#include <future>
#include <vector>
#include <algorithm>
#include <iterator>
#include <QThreadPool>
#include <QString>
#include <cstdint>
#include "../Define/DllExportMacro.hpp"

#include <thread>
export module Thread.Helper;

// <thread> moved from GMF to module body — avoids C1116 stop_token chain in IFC

export namespace ArtifactCore
{
 struct NamedThreadCount {
  QString name;
  int count = 0;
 };

 struct BackgroundThreadPoolSnapshot {
  QString poolName;
  int maxThreadCount = 0;
  int activeThreadCount = 0;
  int expiryTimeoutMs = 0;
 };

 struct ProcessThreadSnapshot {
  int totalThreadCount = 0;
  std::vector<NamedThreadCount> nameCounts;
 };

  QThreadPool& sharedBackgroundThreadPool();
  BackgroundThreadPoolSnapshot sharedBackgroundThreadPoolSnapshot();
  ProcessThreadSnapshot currentProcessThreadSnapshot();
  QString sharedBackgroundThreadPoolDebugString();
  QString currentProcessThreadDebugString();
 
 class ScopedThreadName {
 public:
  explicit ScopedThreadName(const std::string& name);
  explicit ScopedThreadName(const QString& name);
  ~ScopedThreadName();

 private:
  QString previousName_;
 };

 template<typename Iterator, typename Func>
 void execute_parallel_if(Iterator begin, Iterator end, Func&& func, bool multithread)
 {
  if (!multithread) {
   for (auto it = begin; it != end; ++it) {
	func(*it);
   }
   return;
  }

  // 並列実行
  auto length = std::distance(begin, end);
  if (length == 0) return;

  unsigned int threadCount = std::thread::hardware_concurrency();
  if (threadCount == 0) threadCount = 2; // fallback

  // 分割数はスレッド数か要素数の小さい方
  unsigned int chunkSize = static_cast<unsigned int>((length + threadCount - 1) / threadCount);

  std::vector<std::future<void>> futures;
  auto chunkStart = begin;

  for (unsigned int i = 0; i < threadCount && chunkStart != end; ++i) {
   auto chunkEnd = chunkStart;
   unsigned int dist = std::min(chunkSize, static_cast<unsigned int>(std::distance(chunkStart, end)));
   std::advance(chunkEnd, dist);

   futures.emplace_back(std::async(std::launch::async, [chunkStart, chunkEnd, &func]() {
	for (auto it = chunkStart; it != chunkEnd; ++it) {
	 func(*it);
	}
	}));

   chunkStart = chunkEnd;
  }

  for (auto& fut : futures) {
   fut.get();
  }
 }

  void setCurrentThreadName(const std::string& name);
 void setThreadPriorityHigh();
	

}
