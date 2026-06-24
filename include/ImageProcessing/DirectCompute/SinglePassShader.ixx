module;
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
export module ImageProcessing.Shader;
import Graphics.Compute;
import Graphics.GPUcomputeContext;

export namespace ArtifactCore {

 enum class SinglePassShaderParameterType {
  Float,
  Float2,
  Float3,
  Float4,
  Int,
  UInt,
  Bool
 };

 struct SinglePassShaderParameter {
  std::string name;
  SinglePassShaderParameterType type = SinglePassShaderParameterType::Float;
 };

 struct SinglePassShaderResourceNames {
  std::string inputTexture = "InputTexture";
  std::string outputTexture = "OutputTexture";
  std::string sampler = "LinearClampSampler";
  std::string constants = "EffectParams";
 };

 enum class SinglePassShaderBindingKind {
  InputTexture,
  OutputTexture,
  Sampler,
  Constants
 };

 struct SinglePassShaderBinding {
  std::string name;
  SinglePassShaderBindingKind kind = SinglePassShaderBindingKind::InputTexture;
  bool mutablePerDispatch = true;
 };

 struct SinglePassShaderOptions {
  std::string shaderName = "ArtifactSinglePassImageEffect";
  std::string entryPoint = "main";
  std::string pixelFunctionName = "processPixel";
  std::string pixelBody;
  std::vector<SinglePassShaderParameter> parameters;
  SinglePassShaderResourceNames resources;
  int threadGroupSizeX = 8;
  int threadGroupSizeY = 8;
  bool useSampler = true;
  bool includeUv = true;
 };

 struct SinglePassShaderStage {
  std::string name;
  std::string body;
 };

 struct SinglePassShaderChainOptions {
  std::string shaderName = "ArtifactSinglePassImageEffectChain";
  std::string entryPoint = "main";
  std::vector<SinglePassShaderParameter> parameters;
  std::vector<SinglePassShaderStage> stages;
  SinglePassShaderResourceNames resources;
  int threadGroupSizeX = 8;
  int threadGroupSizeY = 8;
  bool useSampler = false;
  bool includeUv = true;
 };

 struct SinglePassShaderSource {
  std::string name;
  std::string entryPoint;
  std::string cacheKey;
  std::string hlsl;
  std::vector<SinglePassShaderBinding> bindings;
  unsigned long long sourceHash = 0;
  int threadGroupSizeX = 8;
  int threadGroupSizeY = 8;
  int dispatchGroupCountX(int width) const;
  int dispatchGroupCountY(int height) const;
  ComputePipelineDesc toComputePipelineDesc() const;
  ComputePipelineDesc toComputePipelineDesc(std::vector<Diligent::ShaderResourceVariableDesc>& variables) const;
 };

 class SinglePassShader {
 private:
  class Impl;
  Impl* impl_;
 public:
  SinglePassShader();
  ~SinglePassShader();
  SinglePassShader(const SinglePassShader&) = delete;
  SinglePassShader& operator=(const SinglePassShader&) = delete;
  SinglePassShader(SinglePassShader&& other) noexcept;
  SinglePassShader& operator=(SinglePassShader&& other) noexcept;
  static SinglePassShaderSource buildImageComputeShader(const SinglePassShaderOptions& options);
  static SinglePassShaderSource buildImageEffectChainComputeShader(const SinglePassShaderChainOptions& options);
  static SinglePassShaderStage makeBrightnessStage(float amount);
  static SinglePassShaderStage makeContrastStage(float amount);
  static SinglePassShaderStage makeSaturationStage(float amount);
  static SinglePassShaderStage makeInvertStage();
  static SinglePassShaderStage makeGrayscaleStage();
  static SinglePassShaderChainOptions makeStandardImageAdjustChain(
   float brightness = 0.0f,
   float contrast = 1.0f,
   float saturation = 1.0f,
   bool invert = false,
   bool grayscale = false);
  static std::unique_ptr<ComputeExecutor> buildComputeExecutor(
   GpuContext& context,
   const SinglePassShaderSource& source,
   std::vector<Diligent::ShaderResourceVariableDesc>& variables,
   bool initializeStaticResources = true);
  static SinglePassShaderSource buildPassthroughImageComputeShader(std::string shaderName = "ArtifactPassthroughImageEffect");
  void addEffect();
  void dispatch();
  void dispatchBlend();
  void batchDispatch();
 };






};
