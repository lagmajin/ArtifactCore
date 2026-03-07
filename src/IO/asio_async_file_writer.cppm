module;
#include <QFile> // Qt のファイルIO用
#include <QFileInfo> 
#include <boost\asio\io_context.hpp>
#include <boost\asio\thread_pool.hpp>
#include <boost\asio\post.hpp>

module asio_async_file_writer;


namespace ArtifactCore {


 class AsioAsyncFileWriterManager::Impl {
 private:

 public:
  explicit Impl(int threadPoolSize, AsioAsyncFileWriterManager* parent);
  ~Impl();

  boost::asio::io_context m_ioContext;
  boost::asio::thread_pool m_threadPool;
  // make_work_guard は io_context::run() がキューが空になっても終了しないようにする
  boost::asio::executor_work_guard<boost::asio::io_context::executor_type> m_workGuard;
  AsioAsyncFileWriterManager* m_parent; // 親 AsioAsyncFileWriterManager へのポインタ (シグナル発行用)
 };

 AsioAsyncFileWriterManager::Impl::Impl(int threadPoolSize, AsioAsyncFileWriterManager* parent) : m_threadPool(threadPoolSize),
  m_workGuard(boost::asio::make_work_guard(m_ioContext)),
  m_parent(parent)
 {
  // io_context を m_threadPool のスレッドで実行開始
  for (int i = 0; i < threadPoolSize; ++i) {
   boost::asio::post(m_threadPool, [this]() {
	m_ioContext.run();
	});
  }
  qDebug() << "AsioAsyncFileWriterManager::Impl initialized with" << threadPoolSize << "threads.";
 }

 AsioAsyncFileWriterManager::Impl::~Impl()
 {
  // workGuard をリセットして io_context::run() が終了できるようにする
  m_workGuard.reset();
  // io_context が終了するのを待つ (すべての処理が完了するのを待つ)
  m_ioContext.stop(); // io_context に停止を通知
  m_threadPool.join(); // スレッドプール内のスレッドがすべて終了するのを待つ
  qDebug() << "AsioAsyncFileWriterManager::Impl shut down.";
 }

 AsioAsyncFileWriterManager::AsioAsyncFileWriterManager()
 {

 }



 AsioAsyncFileWriterManager::~AsioAsyncFileWriterManager()
 {

 }

 

};