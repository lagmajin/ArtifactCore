module;
#include <utility>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <DiligentCore/Graphics/GraphicsEngine/interface/GraphicsTypes.h>

module ImageProcessing.Shader;
import Graphics.Compute;

namespace ArtifactCore {
 using namespace Diligent;

 class SinglePassShader::Impl {
 private:

 public:

 };

 namespace {
  std::string sanitizeIdentifier(std::string value, const char* fallback)
  {
   if (value.empty())
   {
    value = fallback;
   }

   for (char& ch : value)
   {
    const unsigned char c = static_cast<unsigned char>(ch);
    if (!std::isalnum(c) && ch != '_')
    {
     ch = '_';
    }
   }

   const unsigned char first = static_cast<unsigned char>(value.front());
   if (!std::isalpha(first) && value.front() != '_')
   {
    value.insert(value.begin(), '_');
   }
   return value;
  }

  std::string parameterTypeName(SinglePassShaderParameterType type)
  {
   switch (type)
   {
   case SinglePassShaderParameterType::Float:
    return "float";
   case SinglePassShaderParameterType::Float2:
    return "float2";
   case SinglePassShaderParameterType::Float3:
    return "float3";
   case SinglePassShaderParameterType::Float4:
    return "float4";
   case SinglePassShaderParameterType::Int:
    return "int";
   case SinglePassShaderParameterType::UInt:
    return "uint";
   case SinglePassShaderParameterType::Bool:
    return "bool";
   }
   return "float";
  }

  int normalizedThreadGroupSize(int value)
  {
   return std::clamp(value, 1, 32);
  }

  int dispatchGroupCount(int extent, int threadGroupSize)
  {
   if (extent <= 0)
   {
    return 0;
   }
   return (extent + threadGroupSize - 1) / threadGroupSize;
  }

  unsigned long long stableHash(const std::string& text)
  {
   unsigned long long hash = 1469598103934665603ull;
   for (const unsigned char ch : text)
   {
    hash ^= ch;
    hash *= 1099511628211ull;
   }
   return hash;
  }

  std::string makeCacheKey(const SinglePassShaderSource& source)
  {
   std::ostringstream key;
   key << source.name << ":";
   key << source.entryPoint << ":";
   key << source.threadGroupSizeX << "x" << source.threadGroupSizeY << ":";
   key << source.sourceHash;
   return key.str();
  }

  void appendIndented(std::ostringstream& out, const std::string& text, const char* indent)
  {
   std::istringstream lines(text);
   std::string line;
   while (std::getline(lines, line))
   {
    out << indent << line << "\n";
   }
  }

  std::string buildChainPixelBody(const std::vector<SinglePassShaderStage>& stages)
  {
   if (stages.empty())
   {
    return "return src;";
   }

   std::ostringstream body;
   body << "float4 color = src;\n";
   for (const SinglePassShaderStage& stage : stages)
   {
      const std::string stageName = sanitizeIdentifier(stage.name, "stage");
      body << "\n";
      body << "// stage: " << stageName << "\n";
      appendIndented(body, stage.body, "");
   }
   body << "\nreturn color;";
   return body.str();
  }

  std::string makeStageBody(const std::string& expression)
  {
   return "color = " + expression + ";";
  }
 }

 int SinglePassShaderSource::dispatchGroupCountX(int width) const
 {
  return dispatchGroupCount(width, threadGroupSizeX);
 }

 int SinglePassShaderSource::dispatchGroupCountY(int height) const
 {
  return dispatchGroupCount(height, threadGroupSizeY);
 }

 ComputePipelineDesc SinglePassShaderSource::toComputePipelineDesc() const
 {
  ComputePipelineDesc desc;
  desc.name = cacheKey.empty() ? name.c_str() : cacheKey.c_str();
  desc.shaderSource = hlsl.c_str();
  desc.entryPoint = entryPoint.c_str();
  desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
  return desc;
 }

 ComputePipelineDesc SinglePassShaderSource::toComputePipelineDesc(std::vector<Diligent::ShaderResourceVariableDesc>& variables) const
 {
  variables.clear();
  variables.reserve(bindings.size());
  for (const SinglePassShaderBinding& binding : bindings)
  {
   auto variableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
   if (binding.mutablePerDispatch)
   {
    variableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
   }

   switch (binding.kind)
   {
   case SinglePassShaderBindingKind::InputTexture:
   case SinglePassShaderBindingKind::OutputTexture:
    variables.push_back({SHADER_TYPE_COMPUTE, binding.name.c_str(), variableType});
    break;
   case SinglePassShaderBindingKind::Sampler:
    variables.push_back({SHADER_TYPE_COMPUTE, binding.name.c_str(), SHADER_RESOURCE_VARIABLE_TYPE_STATIC});
    break;
   case SinglePassShaderBindingKind::Constants:
    variables.push_back({SHADER_TYPE_COMPUTE, binding.name.c_str(), SHADER_RESOURCE_VARIABLE_TYPE_STATIC});
    break;
   }
  }

  ComputePipelineDesc desc;
  desc.name = cacheKey.empty() ? name.c_str() : cacheKey.c_str();
  desc.shaderSource = hlsl.c_str();
  desc.entryPoint = entryPoint.c_str();
  desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
  desc.variables = variables.empty() ? nullptr : variables.data();
  desc.variableCount = static_cast<Uint32>(variables.size());
  return desc;
 }

 SinglePassShader::SinglePassShader()
  : impl_(new Impl())
 {

 }
 SinglePassShader::~SinglePassShader()
 {
  delete impl_;
 }

 SinglePassShader::SinglePassShader(SinglePassShader&& other) noexcept
  : impl_(std::exchange(other.impl_, nullptr))
 {
 }

 SinglePassShader& SinglePassShader::operator=(SinglePassShader&& other) noexcept
 {
  if (this != &other)
  {
   delete impl_;
   impl_ = std::exchange(other.impl_, nullptr);
  }
  return *this;
 }

 SinglePassShaderSource SinglePassShader::buildImageComputeShader(const SinglePassShaderOptions& options)
 {
  if (options.pixelBody.empty())
  {
   throw std::invalid_argument("SinglePassShaderOptions::pixelBody must not be empty.");
  }

  SinglePassShaderSource source;
  source.name = sanitizeIdentifier(options.shaderName, "ArtifactSinglePassImageEffect");
  source.entryPoint = sanitizeIdentifier(options.entryPoint, "main");
  source.threadGroupSizeX = normalizedThreadGroupSize(options.threadGroupSizeX);
  source.threadGroupSizeY = normalizedThreadGroupSize(options.threadGroupSizeY);

  const std::string inputTexture = sanitizeIdentifier(options.resources.inputTexture, "InputTexture");
  const std::string outputTexture = sanitizeIdentifier(options.resources.outputTexture, "OutputTexture");
  const std::string sampler = sanitizeIdentifier(options.resources.sampler, "LinearClampSampler");
  const std::string constants = sanitizeIdentifier(options.resources.constants, "EffectParams");
  const std::string pixelFunctionName = sanitizeIdentifier(options.pixelFunctionName, "processPixel");

  source.bindings.push_back({inputTexture, SinglePassShaderBindingKind::InputTexture, true});
  source.bindings.push_back({outputTexture, SinglePassShaderBindingKind::OutputTexture, true});
  if (options.useSampler)
  {
   source.bindings.push_back({sampler, SinglePassShaderBindingKind::Sampler, false});
  }
  if (!options.parameters.empty())
  {
   source.bindings.push_back({constants, SinglePassShaderBindingKind::Constants, false});
  }

  std::ostringstream hlsl;
  hlsl << "// ArtifactStudio generated single-pass image compute shader\n";
  hlsl << "// Name: " << source.name << "\n\n";
  hlsl << "Texture2D<float4> " << inputTexture << " : register(t0);\n";
  hlsl << "RWTexture2D<float4> " << outputTexture << " : register(u0);\n";
  if (options.useSampler)
  {
   hlsl << "SamplerState " << sampler << " : register(s0);\n";
  }

  if (!options.parameters.empty())
  {
   hlsl << "\ncbuffer " << constants << " : register(b0)\n{\n";
   for (const SinglePassShaderParameter& parameter : options.parameters)
   {
    hlsl << "    " << parameterTypeName(parameter.type) << " "
         << sanitizeIdentifier(parameter.name, "param") << ";\n";
   }
   hlsl << "};\n";
  }

  hlsl << "\nfloat4 " << pixelFunctionName << "(float4 src, uint2 pixel";
  if (options.includeUv)
  {
   hlsl << ", float2 uv";
  }
  hlsl << ")\n{\n";
  appendIndented(hlsl, options.pixelBody, "    ");
  hlsl << "}\n\n";

  hlsl << "[numthreads(" << source.threadGroupSizeX << ", " << source.threadGroupSizeY << ", 1)]\n";
  hlsl << "void " << source.entryPoint << "(uint3 dispatchThreadId : SV_DispatchThreadID)\n{\n";
  hlsl << "    uint width;\n";
  hlsl << "    uint height;\n";
  hlsl << "    " << outputTexture << ".GetDimensions(width, height);\n";
  hlsl << "    uint2 pixel = dispatchThreadId.xy;\n";
  hlsl << "    if (pixel.x >= width || pixel.y >= height)\n";
  hlsl << "    {\n";
  hlsl << "        return;\n";
  hlsl << "    }\n\n";
  hlsl << "    float4 src = " << inputTexture << ".Load(int3(pixel, 0));\n";
  if (options.includeUv)
  {
   hlsl << "    float2 uv = (float2(pixel) + 0.5f) / float2(width, height);\n";
   hlsl << "    " << outputTexture << "[pixel] = " << pixelFunctionName << "(src, pixel, uv);\n";
  }
  else
  {
   hlsl << "    " << outputTexture << "[pixel] = " << pixelFunctionName << "(src, pixel);\n";
  }
  hlsl << "}\n";

  source.hlsl = hlsl.str();
  source.sourceHash = stableHash(source.hlsl);
  source.cacheKey = makeCacheKey(source);
  return source;
 }

 SinglePassShaderSource SinglePassShader::buildImageEffectChainComputeShader(const SinglePassShaderChainOptions& options)
 {
  SinglePassShaderOptions singlePass;
  singlePass.shaderName = options.shaderName;
  singlePass.entryPoint = options.entryPoint;
  singlePass.pixelFunctionName = "processEffectChainPixel";
  singlePass.pixelBody = buildChainPixelBody(options.stages);
  singlePass.parameters = options.parameters;
  singlePass.resources = options.resources;
  singlePass.threadGroupSizeX = options.threadGroupSizeX;
  singlePass.threadGroupSizeY = options.threadGroupSizeY;
  singlePass.useSampler = options.useSampler;
  singlePass.includeUv = options.includeUv;
  return buildImageComputeShader(singlePass);
 }

 SinglePassShaderStage SinglePassShader::makeBrightnessStage(float amount)
 {
  SinglePassShaderStage stage;
  stage.name = "brightness";
  stage.body = makeStageBody("float4(color.rgb + float3(" + std::to_string(amount) + "f, " + std::to_string(amount) + "f, " + std::to_string(amount) + "f), color.a)");
  return stage;
 }

 SinglePassShaderStage SinglePassShader::makeContrastStage(float amount)
 {
  SinglePassShaderStage stage;
  stage.name = "contrast";
  const std::string a = std::to_string(amount) + "f";
  stage.body = makeStageBody("float4((color.rgb - 0.5f) * " + a + " + 0.5f, color.a)");
  return stage;
 }

 SinglePassShaderStage SinglePassShader::makeSaturationStage(float amount)
 {
  SinglePassShaderStage stage;
  stage.name = "saturation";
  const std::string a = std::to_string(amount) + "f";
  stage.body = makeStageBody("float4(lerp(dot(color.rgb, float3(0.2126f, 0.7152f, 0.0722f)).xxx, color.rgb, " + a + "), color.a)");
  return stage;
 }

 SinglePassShaderStage SinglePassShader::makeInvertStage()
 {
  SinglePassShaderStage stage;
  stage.name = "invert";
  stage.body = makeStageBody("float4(1.0f - color.rgb, color.a)");
  return stage;
 }

 SinglePassShaderStage SinglePassShader::makeGrayscaleStage()
 {
  SinglePassShaderStage stage;
  stage.name = "grayscale";
  stage.body = makeStageBody("float4(dot(color.rgb, float3(0.2126f, 0.7152f, 0.0722f)).xxx, color.a)");
  return stage;
 }

 SinglePassShaderChainOptions SinglePassShader::makeStandardImageAdjustChain(
  float brightness,
  float contrast,
  float saturation,
  bool invert,
  bool grayscale)
 {
  SinglePassShaderChainOptions options;
  if (brightness != 0.0f)
  {
   options.stages.push_back(makeBrightnessStage(brightness));
  }
  if (contrast != 1.0f)
  {
   options.stages.push_back(makeContrastStage(contrast));
  }
  if (saturation != 1.0f)
  {
   options.stages.push_back(makeSaturationStage(saturation));
  }
  if (invert)
  {
   options.stages.push_back(makeInvertStage());
  }
  if (grayscale)
  {
   options.stages.push_back(makeGrayscaleStage());
  }
  return options;
 }

 std::unique_ptr<ComputeExecutor> SinglePassShader::buildComputeExecutor(
  GpuContext& context,
  const SinglePassShaderSource& source,
  std::vector<Diligent::ShaderResourceVariableDesc>& variables,
  bool initializeStaticResources)
 {
  auto executor = std::make_unique<ComputeExecutor>(context);
  const ComputePipelineDesc desc = source.toComputePipelineDesc(variables);
  if (!executor->build(desc))
  {
   return {};
  }
  if (!executor->createShaderResourceBinding(initializeStaticResources))
  {
   return {};
  }
  return executor;
 }

 SinglePassShaderSource SinglePassShader::buildPassthroughImageComputeShader(std::string shaderName)
 {
  SinglePassShaderOptions options;
  options.shaderName = std::move(shaderName);
  options.pixelBody = "return src;";
  options.useSampler = false;
  options.includeUv = false;
  return buildImageComputeShader(options);
 }

 void SinglePassShader::addEffect()
 {
 }

 

 void SinglePassShader::dispatch()
 {

 }

 void SinglePassShader::dispatchBlend()
 {
 }

 void SinglePassShader::batchDispatch()
 {
 }

};
