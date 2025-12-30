module;
export module RenderQueue;

import std;
import Utils.Id;



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