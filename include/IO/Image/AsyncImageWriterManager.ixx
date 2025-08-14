module;
#include <QUuid>
#include <QImage>
#include <QString>
#include <QDateTime> 

#include <boost/asio.hpp>
//#include <boost/>

export module IO.Async.ImageWriterManager;

import std;

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



