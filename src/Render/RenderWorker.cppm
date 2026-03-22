module;
#include <tbb/tbb.h>
#include <QRunnable>
module Render.Worker;



namespace ArtifactCore {

 class RenderWorker::Impl {
 private:

 public:
  Impl();
  ~Impl();
  void writeToImageFile();

 };

 RenderWorker::Impl::Impl()
 {

 }

 RenderWorker::Impl::~Impl()
 {

 }

 RenderWorker::RenderWorker()
 {
  setAutoDelete(true);
 }

 RenderWorker::~RenderWorker()
 {

 }

 void RenderWorker::run()
 {
  
 }

};