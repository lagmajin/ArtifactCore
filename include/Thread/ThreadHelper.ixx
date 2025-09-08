module;
#include <vector>
#include <thread>
#include <future>
#include <functional>

#include <QString>

#include "../Define/DllExportMacro.hpp"

export module Thread.Helper;

export namespace ArtifactCore
{

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

 LIBRARY_DLL_API void setCurrentThreadName(const QString& name);


}
