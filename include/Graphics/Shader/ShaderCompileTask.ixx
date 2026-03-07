
module ;
#include <DiligentCore/Graphics/GraphicsEngine/interface/Shader.h>
export module Graphics.Shader.Compile.Task;


export namespace ArtifactCore {
 
 using namespace Diligent;

 struct ShaderCompileTask {
  ShaderCreateInfo info;
  IShader** out=nullptr;
 };


};