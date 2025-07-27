module;
#include <boost/asio.hpp>
#include <boost/asio/thread_pool.hpp> 


#include <QObject>
#include <QVector>
#include <QByteArray>
export module asio_async_file_writer;

import std;


export namespace ArtifactCore {
 // ファイル書き込み完了時の結果構造体
 struct WriteResult {
  bool success;
  QString filePath;
  std::size_t bytesWritten;
  QString errorMessage;

  WriteResult(bool s = false, const QString& path = "", std::size_t bytes = 0, const QString& msg = "")
   : success(s), filePath(path), bytesWritten(bytes), errorMessage(msg) {
  }
 };

 // ファイル書き込み完了時のコールバック型
 using WriteCompletionCallback = std::function<void(const WriteResult&)>;

 class AsioAsyncFileWriterManager:public QObject {
 private:
  class Impl;
  Impl* impl_;
 public:
  AsioAsyncFileWriterManager();
  ~AsioAsyncFileWriterManager();
  void writeFileAsync(const QString& filePath, const QByteArray& data, WriteCompletionCallback callback = nullptr);
  void writeFileAsync(const QString& filePath, const QVector<unsigned char>& data, WriteCompletionCallback callback = nullptr);

  // コールバックベースの書き込み (std::vector<unsigned char>)
  void writeFileAsync(const QString& filePath, const std::vector<unsigned char>& data, WriteCompletionCallback callback = nullptr);

 };



};