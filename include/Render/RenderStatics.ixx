module;
#include <QElapsedTimer>
#include <string>
#include <unordered_map>
export module Render.Statics;

export namespace ArtifactCore
{
 class ProfileResult
 {
 private:

 public:
  ProfileResult();

 };

 class RendererProfiler
 {
 public:
  RendererProfiler();
  ~RendererProfiler();

  void BeginCPU(const std::string& name);
  void EndCPU(const std::string& name);
  void Reset();

 private:
  struct Impl;
  Impl* impl_;  // 生ポインタでPImpl
 };








};