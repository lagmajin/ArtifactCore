module;
#include <QUuid>
#include <QImage>
#include <QString>
#include <QDateTime> 

#include <boost/asio.hpp>
//#include <boost/>

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
  void enqueueWriter(const QString& filepath,sptrRawImage image);
  bool hasRenderQueue() const;
 };


};



