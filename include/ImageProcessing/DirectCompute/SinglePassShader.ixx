module;
export module ImageProcessing.Shader;

import std;

export namespace ArtifactCore {

 class SinglePassShader {
 private:
  class Impl;
  Impl* impl_;
 public:
  SinglePassShader();
  ~SinglePassShader();
  void dispatch();
  void dispatchBlend();
  void batchDispatch();
 };






};