module;
#include <boost/asio.hpp>
//#include <boost/>

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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <thread>
#include <QString>
export module IO.Async.ImageWriterManager;

import Image;



export namespace ArtifactCore {

 class AsyncImageWriterManager {
 private:
  class Impl;
  Impl* impl_;
  AsyncImageWriterManager(const AsyncImageWriterManager&) = delete;
  AsyncImageWriterManager& operator=(const AsyncImageWriterManager&) = delete;


 public:
  AsyncImageWriterManager();
  ~AsyncImageWriterManager();
  void enqueueWriter(const QString& filepath,RawImagePtr image);
  bool hasRenderQueue() const;

 };

 typedef std::shared_ptr<AsyncImageWriterManager> AsyncImageWriteManagerPtr;

	AsyncImageWriteManagerPtr makeImageWriterManager()
 {
  return std::make_shared<AsyncImageWriterManager>();
 }

};



