module;
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <utility>

export module VST3.Interfaces;

import Core.ArtifactString;

// ---------------------------------------------------------------------------
// Self-contained Steinberg VST3 hosting ABI (host side subset).
//
// The declarations below follow the Steinberg VST 3 pluginterfaces headers
// (base/ftypes.h, base/funknown.h, base/ipluginbase.h, base/ibstream.h,
// vst/vsttypes.h, vst/ivstcomponent.h, vst/ivstaudioprocessor.h,
// vst/ivsteditcontroller.h, vst/ivstparameterchanges.h,
// vst/ivsthostapplication.h) so that objects created by third party plug-ins
// can be reached through the very same virtual tables without vendoring the
// SDK headers into this repository.
//
// Rules that must not be broken (they are part of the ABI):
//  * FUnknown has NO virtual destructor: the vtable only contains
//    queryInterface / addRef / release.
//  * The virtual method order inside every interface is fixed and must stay
//    identical to the SDK headers.
//  * struct field order and array sizes are fixed (PFactoryInfo, PClassInfo,
//    BusInfo, ParameterInfo, ProcessSetup, AudioBusBuffers, ProcessData).
//  * On Windows x64 the SDK builds with #pragma pack(16) which for these
//    structs is identical to the default natural alignment, so no packing
//    pragma is required here.
// ---------------------------------------------------------------------------

export namespace Steinberg {

// --- basic types (base/ftypes.h) ---
using int8 = char;
using uint8 = std::uint8_t;
using int16 = std::int16_t;
using uint16 = std::uint16_t;
using int32 = std::int32_t;
using uint32 = std::uint32_t;
using int64 = std::int64_t;
using uint64 = std::uint64_t;
using TSize = int64;
using TBool = uint8;
using tresult = int32;
using char8 = char;
using char16 = char16_t;
using FIDString = const char8*;

// --- VST types (vst/vsttypes.h) ---
using TUID = char8[16];
using TChar = char16;
using String128 = TChar[128];
using ParamID = uint32;
using ParamValue = double;
using UnitID = int32;
using SpeakerArrangement = uint64;
using SampleRate = double;
using Sample32 = float;
using Sample64 = double;
using MediaType = int32;
using BusDirection = int32;
using BusType = int32;
using IoMode = int32;

// --- result codes (base/funknown.h, COM_COMPATIBLE / Windows) ---
constexpr tresult kNoResult = static_cast<tresult>(0x80000000u);
constexpr tresult kResultOk = 0;
constexpr tresult kResultTrue = kResultOk;
constexpr tresult kResultFalse = 1;
constexpr tresult kInvalidArgument = static_cast<tresult>(0x80070057u);
constexpr tresult kNotImplemented = static_cast<tresult>(0x80004001u);
constexpr tresult kInternalError = static_cast<tresult>(0x80004005u);
constexpr tresult kNotInitialized = static_cast<tresult>(0x8000FFFFu);
constexpr tresult kOutOfMemory = static_cast<tresult>(0x8007000Eu);
constexpr tresult kNoInterface = static_cast<tresult>(0x80004002u);

// 4 unsigned 32-bit values -> 16 byte TUID, identical to the COM_COMPATIBLE
// INLINE_UID macro / FUID::from4Int of the SDK.
void makeTUID(uint32 l1, uint32 l2, uint32 l3, uint32 l4, TUID out);

// 32 hex character string form (the form FUID::toString produces and
// FUID::fromString accepts). Used for IPluginFactory::createInstance.
bool tuidFromString(const char8* text, TUID out);
void tuidToString(const TUID uid, char8 out[33]);
bool tuidEquals(const TUID a, const TUID b);

// Interface IIDs in string form. The returned pointers stay valid for the
// lifetime of the process.
FIDString iidStringFUnknown();
FIDString iidStringIBStream();
FIDString iidStringIPluginBase();
FIDString iidStringIPluginFactory();

namespace Vst {
FIDString iidStringIComponent();
FIDString iidStringIAudioProcessor();
FIDString iidStringIEditController();
FIDString iidStringIParameterChanges();
FIDString iidStringIParamValueQueue();
FIDString iidStringIHostApplication();
}

class IBStream;
struct ProcessContext;

// Vst namespace interfaces used by ProcessData (opaque to this host).
namespace Vst {
class IParameterChanges;
class IEventList;
class IPlugView;
} // namespace Vst

// base/funknown.h
class FUnknown {
public:
    virtual tresult queryInterface(const TUID _iid, void** obj) = 0;
    virtual uint32 addRef() = 0;
    virtual uint32 release() = 0;
};

// base/ipluginbase.h
class IPluginBase : public FUnknown {
public:
    virtual tresult initialize(FUnknown* context) = 0;
    virtual tresult terminate() = 0;
};

// base/ibstream.h
class IBStream : public FUnknown {
public:
    enum IStreamSeekMode { kIBSeekSet = 0, kIBSeekCur, kIBSeekEnd };
    virtual tresult read(void* buffer, int32 numBytes, int32* numBytesRead = nullptr) = 0;
    virtual tresult write(void* buffer, int32 numBytes, int32* numBytesWritten = nullptr) = 0;
    virtual tresult seek(int64 pos, int32 mode, int64* result = nullptr) = 0;
    virtual tresult tell(int64* pos) = 0;
};

// base/ipluginbase.h
struct PFactoryInfo {
    enum FactoryFlags {
        kNoFlags = 0,
        kClassesDiscardable = 1 << 0,
        kLicenseCheck = 1 << 1,
        kComponentNonDiscardable = 1 << 3,
        kUnicode = 1 << 4
    };
    enum { kURLSize = 256, kEmailSize = 128, kNameSize = 64 };

    char8 vendor[kNameSize];
    char8 url[kURLSize];
    char8 email[kEmailSize];
    int32 flags;
};

// base/ipluginbase.h
struct PClassInfo {
    enum ClassCardinality { kManyInstances = 0x7FFFFFFF };
    enum { kCategorySize = 32, kNameSize = 64 };

    TUID cid;
    int32 cardinality;
    char8 category[kCategorySize];
    char8 name[kNameSize];
};

// base/ipluginbase.h
class IPluginFactory : public FUnknown {
public:
    virtual tresult getFactoryInfo(PFactoryInfo* info) = 0;
    virtual int32 countClasses() = 0;
    virtual tresult getClassInfo(int32 index, PClassInfo* info) = 0;
    virtual tresult createInstance(FIDString cid, FIDString _iid, void** obj) = 0;
};

// ---------------------------------------------------------------------------
// VST (vst/vsttypes.h, vst/ivstcomponent.h, vst/ivstaudioprocessor.h)
// ---------------------------------------------------------------------------
namespace Vst {

constexpr int32 kDefaultFactoryFlags = PFactoryInfo::kUnicode;
constexpr FIDString kVstAudioEffectClass = "Audio Module Class";
constexpr FIDString kVstComponentControllerClass = "Component Controller Class";

enum MediaTypes { kAudio = 0, kEvent, kNumMediaTypes };
enum BusDirections { kInput = 0, kOutput };
enum BusTypes { kMain = 0, kAux };
enum IoModes { kSimple = 0, kAdvanced, kOfflineProcessing };
enum SymbolicSampleSizes { kSample32 = 0, kSample64 };
enum ProcessModes { kRealtime = 0, kPrefetch, kOffline };
constexpr uint32 kNoTail = 0;
constexpr uint32 kInfiniteTail = 0x7FFFFFFFu;

// vst/ivstcomponent.h — field order is ABI.
struct BusInfo {
    MediaType mediaType;
    BusDirection direction;
    int32 channelCount;
    String128 name;
    BusType busType;
    uint32 flags;

    enum BusFlags { kDefaultActive = 1 << 0, kIsControlVoltage = 1 << 1 };
};

// vst/ivstcomponent.h
struct RoutingInfo {
    MediaType mediaType;
    int32 busIndex;
    int32 channel;
};

// vst/ivstaudioprocessor.h — field order is ABI.
struct ProcessSetup {
    int32 processMode;
    int32 symbolicSampleSize;
    int32 maxSamplesPerBlock;
    SampleRate sampleRate;
};

// vst/ivstaudioprocessor.h — field order is ABI.
struct AudioBusBuffers {
    AudioBusBuffers() : numChannels(0), silenceFlags(0), channelBuffers64(nullptr) {}

    int32 numChannels;
    uint64 silenceFlags;
    union {
        Sample32** channelBuffers32;
        Sample64** channelBuffers64;
    };
};

// vst/ivstaudioprocessor.h — field order is ABI.
struct ProcessData {
    ProcessData()
        : processMode(0), symbolicSampleSize(kSample32), numSamples(0), numInputs(0),
          numOutputs(0), inputs(nullptr), outputs(nullptr), inputParameterChanges(nullptr),
          outputParameterChanges(nullptr), inputEvents(nullptr), outputEvents(nullptr),
          processContext(nullptr) {}

    int32 processMode;
    int32 symbolicSampleSize;
    int32 numSamples;
    int32 numInputs;
    int32 numOutputs;
    AudioBusBuffers* inputs;
    AudioBusBuffers* outputs;

    Vst::IParameterChanges* inputParameterChanges;
    Vst::IParameterChanges* outputParameterChanges;
    Vst::IEventList* inputEvents;
    Vst::IEventList* outputEvents;
    ProcessContext* processContext;
};

// vst/ivstcomponent.h — IComponent methods extend IPluginBase in this order.
class IComponent : public IPluginBase {
public:
    virtual tresult getControllerClassId(TUID classId) = 0;
    virtual tresult setIoMode(IoMode mode) = 0;
    virtual int32 getBusCount(MediaType type, BusDirection dir) = 0;
    virtual tresult getBusInfo(MediaType type, BusDirection dir, int32 index, BusInfo& bus) = 0;
    virtual tresult getRoutingInfo(RoutingInfo& inInfo, RoutingInfo& outInfo) = 0;
    virtual tresult activateBus(MediaType type, BusDirection dir, int32 index, TBool state) = 0;
    virtual tresult setActive(TBool state) = 0;
    virtual tresult setState(IBStream* state) = 0;
    virtual tresult getState(IBStream* state) = 0;
};

// vst/ivstaudioprocessor.h — methods extend FUnknown in this order.
class IAudioProcessor : public FUnknown {
public:
    virtual tresult setBusArrangements(SpeakerArrangement* inputs, int32 numIns,
                                       SpeakerArrangement* outputs, int32 numOuts) = 0;
    virtual tresult getBusArrangement(BusDirection dir, int32 index, SpeakerArrangement& arr) = 0;
    virtual tresult canProcessSampleSize(int32 symbolicSampleSize) = 0;
    virtual uint32 getLatencySamples() = 0;
    virtual tresult setupProcessing(ProcessSetup& setup) = 0;
    virtual tresult setProcessing(TBool state) = 0;
    virtual tresult process(ProcessData& data) = 0;
    virtual uint32 getTailSamples() = 0;
};

} // namespace Vst

// ---------------------------------------------------------------------------
// Edit controller (vst/ivsteditcontroller.h)
// ---------------------------------------------------------------------------
namespace Vst {

// vst/ivsteditcontroller.h — field order is ABI (strings are UTF-16).
struct ParameterInfo {
    ParamID id;
    String128 title;
    String128 shortTitle;
    String128 units;
    int32 stepCount;
    ParamValue defaultNormalizedValue;
    UnitID unitId;
    int32 flags;

    enum ParameterFlags : int32 {
        kNoFlags = 0,
        kCanAutomate = 1 << 0,
        kIsReadOnly = 1 << 1,
        kIsWrapAround = 1 << 2,
        kIsList = 1 << 3,
        kIsHidden = 1 << 4,
        kIsProgramChange = 1 << 15,
        kIsBypass = 1 << 16
    };
};

// vst/ivstparameterchanges.h — host side implementations.
class IParamValueQueue : public FUnknown {
public:
    virtual ParamID getParameterId() = 0;
    virtual int32 getPointCount() = 0;
    virtual tresult getPoint(int32 index, int32& sampleOffset, ParamValue& value) = 0;
    virtual tresult addPoint(int32 sampleOffset, ParamValue value, int32& index) = 0;
};

class IParameterChanges : public FUnknown {
public:
    virtual int32 getParameterCount() = 0;
    virtual IParamValueQueue* getParameterData(int32 index) = 0;
    virtual IParamValueQueue* addParameterData(const ParamID& id, int32& index) = 0;
};

// vst/ivsthostapplication.h — passed as the context of IPluginBase::initialize.
class IHostApplication : public FUnknown {
public:
    virtual tresult getName(String128 name) = 0;
    virtual tresult createInstance(TUID cid, TUID _iid, void** obj) = 0;
};

// vst/ivsteditcontroller.h — IEditController methods extend IPluginBase in this order.
class IEditController : public IPluginBase {
public:
    virtual tresult setComponentState(IBStream* state) = 0;
    virtual tresult setState(IBStream* state) = 0;
    virtual tresult getState(IBStream* state) = 0;
    virtual int32 getParameterCount() = 0;
    virtual tresult getParameterInfo(int32 paramIndex, ParameterInfo& info) = 0;
    virtual tresult getParamStringByValue(ParamID id, ParamValue valueNormalized, String128 string) = 0;
    virtual tresult getParamValueByString(ParamID id, TChar* string, ParamValue& valueNormalized) = 0;
    virtual ParamValue normalizedParamToPlain(ParamID id, ParamValue valueNormalized) = 0;
    virtual ParamValue plainParamToNormalized(ParamID id, ParamValue plainValue) = 0;
    virtual ParamValue getParamNormalized(ParamID id) = 0;
    virtual tresult setParamNormalized(ParamID id, ParamValue value) = 0;
    virtual tresult setComponentHandler(FUnknown* handler) = 0;
    virtual Vst::IPlugView* createView(FIDString name) = 0;
};

// ---------------------------------------------------------------------------
// Module loader (uses IPluginFactory::createInstance of the plug-in)
// ---------------------------------------------------------------------------
using GetFactoryProc = IPluginFactory* (*)();

class VST3Module {
public:
    VST3Module() = default;
    ~VST3Module() { unload(); }
    VST3Module(const VST3Module&) = delete;
    VST3Module& operator=(const VST3Module&) = delete;

    bool load(const ArtifactCore::String& path);
    void unload();

    IPluginFactory* getFactory() const { return factory_; }
    bool isValid() const { return factory_ != nullptr; }
    const char8* lastError() const { return lastError_.c_str(); }

    // Class enumeration helpers (safe against non conforming factories).
    int32 classCount() const;
    bool classInfo(int32 index, PClassInfo& out) const;
    void* createInstance(FIDString cid, FIDString iid) const;

private:
    void* moduleHandle_ = nullptr;
    GetFactoryProc getFactoryProc_ = nullptr;
    IPluginFactory* factory_ = nullptr;
    std::string lastError_;
};

// ---------------------------------------------------------------------------
// Host side audio effect facade. Owns the component / controller pair and the
// host side helper interfaces (state stream, parameter queues). All buffers
// used by processFloat/processDouble are preallocated in setup(), the process
// call itself performs no allocation.
// ---------------------------------------------------------------------------
class VST3EffectHost {
public:
    VST3EffectHost();
    ~VST3EffectHost();
    VST3EffectHost(const VST3EffectHost&) = delete;
    VST3EffectHost& operator=(const VST3EffectHost&) = delete;

    // classIndex < 0 picks the first "Audio Module Class" (effect) class.
    bool load(const ArtifactCore::String& pluginPath, int32 classIndex = -1);
    void unload();
    bool isLoaded() const;

    // Metadata (UTF-8, may be empty).
    const char8* className() const;
    const char8* vendorName() const;
    int32 inputBusCount() const;
    int32 outputBusCount() const;
    int32 inputChannelCount() const;
    int32 outputChannelCount() const;
    int32 parameterCount() const;
    bool getParameterInfo(int32 index, Vst::ParameterInfo& info) const;
    bool getParameterDisplay(int32 index, String128 text) const;

    // Processing. setup() preallocates the per block buffers.
    bool setup(double sampleRate, int32 maxBlockSize, bool doublePrecision = false);
    bool isActive() const;
    bool isProcessing() const;
    uint32 latencySamples() const;
    uint32 tailSamples() const;
    bool setActive(bool active);
    bool setProcessing(bool processing);

    bool setParameterNormalized(ParamID id, ParamValue value);
    ParamValue parameterNormalized(ParamID id) const;
    bool flushPendingParameters();

    // The caller owns the native parent window and invokes these on the UI
    // thread. The optional callback lets the application resize its container
    // before the plug-in view receives onSize() for plug-in initiated changes.
    using EditorResizeCallback = bool (*)(void* context, int32 width, int32 height);
    bool openEditor(void* nativeParent, void* resizeContext,
                    EditorResizeCallback resizeCallback,
                    int32& width, int32& height, bool& resizable);
    bool resizeEditor(int32 width, int32 height);
    void closeEditor();
    bool hasEditor() const;

    // inputs/outputs must provide inputChannelCount()/outputChannelCount()
    // channel pointers each; buses without requested channels may be nullptr.
    bool processFloat(Sample32* const* inputs, int32 numInputChannels,
                      Sample32* const* outputs, int32 numOutputChannels, int32 numSamples);
    bool processDouble(Sample64* const* inputs, int32 numInputChannels,
                       Sample64* const* outputs, int32 numOutputChannels, int32 numSamples);

    // Component + controller state, bundled into one buffer.
    bool saveState(std::vector<uint8_t>& outState) const;
    bool loadState(const std::vector<uint8_t>& state);

    const char8* lastError() const;

private:
    class Impl;
    Impl* impl_;
};

} // namespace Vst
}
