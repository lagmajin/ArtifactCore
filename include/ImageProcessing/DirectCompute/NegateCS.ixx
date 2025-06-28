
module;
#include "../../Define/DllExportMacro.hpp"
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
export module ImageProcessing:NegateCS;

export namespace ArtifactCore
{
 using namespace Diligent;

 class LIBRARY_DLL_API NegateCS
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  NegateCS(RefCntAutoPtr<IRenderDevice> device);
  ~NegateCS();
  void Process();
 };











};