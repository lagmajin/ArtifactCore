module;
#include <QObject>
#include <QCoreApplication>
module Render.Queue.Manager;

 namespace ArtifactCore {

  class RendererQueueManager::Impl{
  private:

  public:
   //static RendererQueueManager* instance_=nullptr;
  };

  RendererQueueManager::RendererQueueManager(QObject* parent/*=nullptr*/):QObject(parent)
  {

  }


  RendererQueueManager::~RendererQueueManager()
  {

  }
 
  RendererQueueManager& RendererQueueManager::instance()
  {
   static RendererQueueManager s_instance(QCoreApplication::instance());
   return s_instance;
  }








};