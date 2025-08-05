module;
export module Graphics.EngineType;


export namespace ArtifactCore
{
 enum class GraphicsAPI
 {
  Unknown = 0,
  D3D10,
  D3D11,
  D3D12,
  Vulkan,
  OpenGL,
  Metal,
  Software, // fallback or headless
  Count     // 要素数
 };






};