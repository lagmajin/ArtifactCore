module;
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include <string>

export module VST3.Interfaces;

/// VST3 SDK 基本インターフェース定義（骨格）
/// 実運用時は Steinberg VST3 SDK ヘッダを include すること
export namespace Steinberg {

using int16  = int16_t;
using int32  = int32_t;
using int64  = int64_t;
using uint32 = uint32_t;
using float32 = float;
using float64 = double;

using TUID = char[16];
using FUID = TUID;
using UTF8StringPtr = const char*;
using TBool = int32;

constexpr TBool kResultOk = 1;
constexpr TBool kResultFalse = 0;
constexpr TBool kResultTrue = 1;

using tresult = int32;

// FUnknown: 全インターフェースの基底
class FUnknown {
public:
    virtual tresult queryInterface(const TUID& iid, void** iface) = 0;
    virtual uint32 addRef() = 0;
    virtual uint32 release() = 0;
    virtual ~FUnknown() = default;
};

// IPluginBase
class IPluginBase : public FUnknown {
public:
    virtual tresult initialize() = 0;
    virtual tresult terminate() = 0;
};

// バス情報
enum class BusType { kAudio, kEvent };
enum class BusDirection { kInput, kOutput };

struct BusInfo {
    UTF8StringPtr name = "";
    BusType type = BusType::kAudio;
    BusDirection direction = BusDirection::kInput;
    int32 channelCount = 0;
};

// 処理設定
struct ProcessSetup {
    int32 maxSampleRate = 48000;
    int32 maxBlockSize = 512;
    float64 sampleRate = 48000.0;
};

struct AudioBusBuffers {
    int32 numChannels = 0;
    float** channelBuffers = nullptr;
};

struct ProcessData {
    int32 numInputs = 0;
    int32 numOutputs = 0;
    AudioBusBuffers* inputs = nullptr;
    AudioBusBuffers* outputs = nullptr;
    int32 numSamples = 0;
    float64 sampleRate = 0.0;
};

// IAudioProcessor
class IAudioProcessor : public FUnknown {
public:
    virtual uint32 getLatencySamples() = 0;
    virtual tresult setupProcessing(const ProcessSetup& setup) = 0;
    virtual tresult setBusArrangements(
        BusDirection* dirs, int32 numDirs,
        int32 numInputs, int32 numOutputs) = 0;
    virtual tresult process(const ProcessData& data) = 0;
    virtual uint32 getTailSamples() = 0;
};

// パラメータ情報
enum class ParameterFlags : int32 {
    kNoFlags = 0,
    kCanAutomate = 1 << 0,
    kIsReadOnly = 1 << 1,
    kIsWrapAround = 1 << 2,
    kIsList = 1 << 3,
    kIsProgramChange = 1 << 4,
    kIsBypass = 1 << 5,
};

struct ParameterInfo {
    UTF8StringPtr title = "";
    UTF8StringPtr units = "";
    int32 stepCount = 0;
    float32 defaultNormalizedValue = 0.0f;
    ParameterFlags flags = ParameterFlags::kNoFlags;
    int32 id = 0;
};

// IEditController
class IEditController : public FUnknown {
public:
    virtual tresult setComponentState(void* data, int32 size) = 0;
    virtual tresult setState(void* data, int32 size) = 0;
    virtual tresult getState(void** data, int32* size) = 0;
    virtual int32 getParameterCount() = 0;
    virtual tresult getParameterInfo(int32 index, ParameterInfo& info) = 0;
    virtual tresult getParamStringByValue(int32 id, float32 value, char* str, int32 size) = 0;
    virtual tresult getParamValueByString(int32 id, const char* str, float32& value) = 0;
    virtual float32 normalizeParam(int32 id, float32 value) = 0;
    virtual float32 denormalizeParam(int32 id, float32 value) = 0;
    virtual tresult setParamNormalized(int32 id, float32 value) = 0;
    virtual float32 getParamNormalized(int32 id) = 0;
};

// IPluginFactory
class IPluginFactory : public FUnknown {
public:
    virtual int32 countPlugins() = 0;
    virtual tresult getPluginInfo(int32 index, class PClassInfo& info) = 0;
    virtual tresult createInstance(int32 index, const TUID& iid, void** obj) = 0;
};

struct PClassInfo {
    TUID cid = {};
    TUID cid2 = {};
    UTF8StringPtr name = "";
    UTF8StringPtr vendor = "";
    UTF8StringPtr version = "";
    UTF8StringPtr category = "";
};

// 動的ライブラリローダー
using GetFactoryProc = IPluginFactory* (*)();

class VST3Module {
public:
    VST3Module() = default;
    ~VST3Module() { unload(); }

    bool load(const std::string& path);
    void unload();
    IPluginFactory* getFactory() const { return factory_; }
    bool isValid() const { return factory_ != nullptr; }

private:
    void* moduleHandle_ = nullptr;
    GetFactoryProc getFactoryProc_ = nullptr;
    IPluginFactory* factory_ = nullptr;
};

} // namespace Steinberg
