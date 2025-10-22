
module;
#include <QString>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <GL/gl.h>
#include <vector>
#include <string>
#include <dxgi.h>
#include <wrl/client.h>
#include <QString>
module Graphics.GPU.Info;

#pragma comment(lib, "dxgi.lib")

namespace ArtifactCore {
 using Microsoft::WRL::ComPtr;
	

	
 /*
 QString GPUInfo::vendor() const
 {
  const GLubyte* v = glGetString(GL_VENDOR);
  return v ? QString(reinterpret_cast<const char*>(v)) : "UnknownVendor";
 }

 QString GPUInfo::renderer() const
 {
  const GLubyte* r = glGetString(GL_RENDERER);
  return r ? QString(reinterpret_cast<const char*>(r)) : "UnknownRenderer";
 }
 */
}