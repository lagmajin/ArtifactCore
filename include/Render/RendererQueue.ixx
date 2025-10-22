module;
export module RenderQueue;

export namespace ArtifactCore
{

 class RenderQueue
 {
 private:
  class Impl;
  Impl* impl_;

 public:
  RenderQueue();
  ~RenderQueue();
 };



};