module;
#include <boost/asio.hpp>
module IO.Async.ImageWriterManager;

import Image;

namespace ArtifactCore {

 class AsyncImageWriterManager::Impl {
 private:
  boost::asio::io_context io_context_;
  boost::asio::thread_pool thread_pool_; // ワーカーthreadを管理
  std::vector<std::jthread> workers_; // C++20 jthreadで管理を容易に
  std::atomic<bool> stop_processing_{ false };
  //boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
  void performImageWrite(std::string filepath_str, sptrRawImage image);
 public:
  Impl(size_t num_threads = std::thread::hardware_concurrency());
  ~Impl();
  void enqueueImageWrite(const QString& filepath, sptrRawImage image);
 };


 AsyncImageWriterManager::Impl::Impl(size_t num_threads /*= std::thread::hardware_concurrency()*/)
 {

 }

 AsyncImageWriterManager::Impl::~Impl()
 {

 }
 void AsyncImageWriterManager::Impl::enqueueImageWrite(const QString& filepath, sptrRawImage image)
 {
  if (stop_processing_.load()) {
   // マネージャーがシャットダウン中の場合のエラーをログに記録するか、処理します。
   // 例: std::cerr << "Warning: Cannot enqueue image write, manager is shutting down." << std::endl;
   return;
  }
 }

 

 AsyncImageWriterManager::AsyncImageWriterManager()
 {

 }

 AsyncImageWriterManager::~AsyncImageWriterManager()
 {

 }



 void AsyncImageWriterManager::enqueueWriter(const QString& filepath, sptrRawImage image)
 {

 }

};

