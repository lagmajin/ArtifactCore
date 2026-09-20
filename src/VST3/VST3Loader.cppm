module;
#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <new>
#include <string>
#include <vector>
#include <atomic>
#include <utility>

module VST3.Interfaces;

import Core.ArtifactString;

// ---------------------------------------------------------------------------
// TUID <-> string conversion.
// The byte expansion and the 32 hex character string form follow
// pluginterfaces/base/funknown.cpp (FUID::from4Int / toString / fromString)
// with COM_COMPATIBLE enabled, so UIDs round-trip exactly like the SDK.
// ---------------------------------------------------------------------------
namespace Steinberg {

namespace {

bool hexValue(char8 c, uint32& out) {
    if (c >= '0' && c <= '9') { out = static_cast<uint32>(c - '0'); return true; }
    if (c >= 'A' && c <= 'F') { out = static_cast<uint32>(c - 'A' + 10); return true; }
    if (c >= 'a' && c <= 'f') { out = static_cast<uint32>(c - 'a' + 10); return true; }
    return false;
}

bool hexUInt(const char8* text, int digits, uint32& out) {
    uint32 value = 0;
    for (int i = 0; i < digits; ++i) {
        uint32 nibble = 0;
        if (!hexValue(text[i], nibble)) return false;
        value = (value << 4) | nibble;
    }
    out = value;
    return true;
}

uint32 readLe32(const char8* bytes) {
    return static_cast<uint32>(static_cast<uint8>(bytes[0])) |
           (static_cast<uint32>(static_cast<uint8>(bytes[1])) << 8) |
           (static_cast<uint32>(static_cast<uint8>(bytes[2])) << 16) |
           (static_cast<uint32>(static_cast<uint8>(bytes[3])) << 24);
}

uint32 readLe16(const char8* bytes) {
    return static_cast<uint32>(static_cast<uint8>(bytes[0])) |
           (static_cast<uint32>(static_cast<uint8>(bytes[1])) << 8);
}

// Fixed size foreign C string -> bounded UTF-8 std::string. The SDK uses
// NUL padded buffers (strncpy8 convention); never read past the limit.
std::string boundedCString(const char8* text, size_t limit) {
    if (!text || limit == 0) return {};
    size_t length = 0;
    while (length < limit && text[length] != '\0') ++length;
    return std::string(text, length);
}

std::string uidStringFromInts(uint32 l1, uint32 l2, uint32 l3, uint32 l4) {
    char8 uid[16];
    makeTUID(l1, l2, l3, l4, uid);
    char8 text[33];
    tuidToString(uid, text);
    return std::string(text);
}

} // namespace

void makeTUID(uint32 l1, uint32 l2, uint32 l3, uint32 l4, TUID out) {
    out[0]  = static_cast<char8>(l1 & 0x000000FFu);
    out[1]  = static_cast<char8>((l1 & 0x0000FF00u) >> 8);
    out[2]  = static_cast<char8>((l1 & 0x00FF0000u) >> 16);
    out[3]  = static_cast<char8>((l1 & 0xFF000000u) >> 24);
    out[4]  = static_cast<char8>((l2 & 0x00FF0000u) >> 16);
    out[5]  = static_cast<char8>((l2 & 0xFF000000u) >> 24);
    out[6]  = static_cast<char8>(l2 & 0x000000FFu);
    out[7]  = static_cast<char8>((l2 & 0x0000FF00u) >> 8);
    out[8]  = static_cast<char8>((l3 & 0xFF000000u) >> 24);
    out[9]  = static_cast<char8>((l3 & 0x00FF0000u) >> 16);
    out[10] = static_cast<char8>((l3 & 0x0000FF00u) >> 8);
    out[11] = static_cast<char8>(l3 & 0x000000FFu);
    out[12] = static_cast<char8>((l4 & 0xFF000000u) >> 24);
    out[13] = static_cast<char8>((l4 & 0x00FF0000u) >> 16);
    out[14] = static_cast<char8>((l4 & 0x0000FF00u) >> 8);
    out[15] = static_cast<char8>(l4 & 0x000000FFu);
}

void tuidToString(const TUID uid, char8 out[33]) {
    // FUID::toString: "%08X%04X%04X" of the little endian Data1/Data2/Data3
    // followed by the plain hex of bytes 8..15.
    uint32 cursor = 0;
    auto append = [&](uint32 value, int digits) {
        static const char8 kHex[] = "0123456789ABCDEF";
        for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4) {
            out[cursor++] = kHex[(value >> shift) & 0xFu];
        }
    };
    append(readLe32(uid), 8);
    append(readLe16(uid + 4), 4);
    append(readLe16(uid + 6), 4);
    for (int i = 8; i < 16; ++i) {
        append(static_cast<uint32>(static_cast<uint8>(uid[i])), 2);
    }
    out[cursor] = '\0';
}

bool tuidFromString(const char8* text, TUID out) {
    if (!text || std::strlen(text) != 32) return false;
    uint32 data1 = 0, data2 = 0, data3 = 0;
    if (!hexUInt(text, 8, data1) || !hexUInt(text + 8, 4, data2) ||
        !hexUInt(text + 12, 4, data3)) {
        return false;
    }
    out[0] = static_cast<char8>(data1 & 0xFFu);
    out[1] = static_cast<char8>((data1 >> 8) & 0xFFu);
    out[2] = static_cast<char8>((data1 >> 16) & 0xFFu);
    out[3] = static_cast<char8>((data1 >> 24) & 0xFFu);
    out[4] = static_cast<char8>(data2 & 0xFFu);
    out[5] = static_cast<char8>((data2 >> 8) & 0xFFu);
    out[6] = static_cast<char8>(data3 & 0xFFu);
    out[7] = static_cast<char8>((data3 >> 8) & 0xFFu);
    for (int i = 8; i < 16; ++i) {
        uint32 byte = 0;
        if (!hexUInt(text + i * 2, 2, byte)) return false;
        out[i] = static_cast<char8>(byte);
    }
    return true;
}

bool tuidEquals(const TUID a, const TUID b) {
    return std::memcmp(a, b, 16) == 0;
}

// --- verified interface IIDs (DECLARE_CLASS_IID of the SDK headers) ---
FIDString iidStringFUnknown() {
    static const std::string iid =
        uidStringFromInts(0x00000000u, 0x00000000u, 0xC0000000u, 0x00000046u);
    return iid.c_str();
}
FIDString iidStringIBStream() {
    static const std::string iid =
        uidStringFromInts(0xC3BF6EA2u, 0x30994752u, 0x9B6BF990u, 0x1EE33E9Bu);
    return iid.c_str();
}
FIDString iidStringIPluginBase() {
    static const std::string iid =
        uidStringFromInts(0x22888DDBu, 0x156E45AEu, 0x8358B348u, 0x08190625u);
    return iid.c_str();
}
FIDString iidStringIPluginFactory() {
    static const std::string iid =
        uidStringFromInts(0x7A4D811Cu, 0x52114A1Fu, 0xAED9D2EEu, 0x0B43BF9Fu);
    return iid.c_str();
}

namespace Vst {
FIDString iidStringIComponent() {
    static const std::string iid =
        uidStringFromInts(0xE831FF31u, 0xF2D54301u, 0x928EBBEEu, 0x25697802u);
    return iid.c_str();
}
FIDString iidStringIAudioProcessor() {
    static const std::string iid =
        uidStringFromInts(0x42043F99u, 0xB7DA453Cu, 0xA569E79Du, 0x9AAEC33Du);
    return iid.c_str();
}
FIDString iidStringIEditController() {
    static const std::string iid =
        uidStringFromInts(0xDCD7BBE3u, 0x7742448Du, 0xA874AACCu, 0x979C759Eu);
    return iid.c_str();
}
FIDString iidStringIParameterChanges() {
    static const std::string iid =
        uidStringFromInts(0xA4779663u, 0x0BB64A56u, 0xB44384A8u, 0x466FEB9Du);
    return iid.c_str();
}
FIDString iidStringIParamValueQueue() {
    static const std::string iid =
        uidStringFromInts(0x01263A18u, 0xED074F6Fu, 0x98C9D356u, 0x4686F9BAu);
    return iid.c_str();
}
FIDString iidStringIHostApplication() {
    static const std::string iid =
        uidStringFromInts(0x58E595CCu, 0xDB2D4969u, 0x8B6AAF8Cu, 0x36A664E5u);
    return iid.c_str();
}
} // namespace Vst

// ---------------------------------------------------------------------------
// Host side helper interfaces
// ---------------------------------------------------------------------------
namespace {

class AtomicRefCount {
public:
    uint32 fetchAdd(int32 delta) { return value_.fetch_add(delta) + static_cast<uint32>(delta); }
    uint32 fetchSub(int32 delta) { return value_.fetch_sub(delta) - static_cast<uint32>(delta); }
    uint32 value() const { return value_.load(); }

private:
    std::atomic<uint32> value_{1};
};

// Null host application passed to IPluginBase::initialize.
class HostApplication final : public Vst::IHostApplication {
public:
    HostApplication() {
        tuidFromString(iidStringFUnknown(), iidFUnknown_);
        tuidFromString(Vst::iidStringIHostApplication(), iidSelf_);
    }

    tresult queryInterface(const TUID _iid, void** obj) override {
        if (!_iid || !obj) return kInvalidArgument;
        if (tuidEquals(_iid, iidFUnknown_) || tuidEquals(_iid, iidSelf_)) {
            *obj = static_cast<FUnknown*>(this);
            addRef();
            return kResultOk;
        }
        *obj = nullptr;
        return kNoInterface;
    }
    uint32 addRef() override { return refCount_.fetchAdd(1); }
    uint32 release() override {
        const uint32 remaining = refCount_.fetchSub(1);
        if (remaining == 1) delete this;
        return remaining - 1;
    }
    tresult getName(String128 name) override {
        if (!name) return kInvalidArgument;
        std::memset(name, 0, sizeof(String128));
        static const char16 kHostName[] = u"ArtifactStudio";
        std::memcpy(name, kHostName, sizeof(kHostName));
        return kResultOk;
    }
    tresult createInstance(TUID, TUID, void** obj) override {
        if (obj) *obj = nullptr;
        return kNotImplemented;
    }

private:
    TUID iidFUnknown_{};
    TUID iidSelf_{};
    AtomicRefCount refCount_;
};

// Memory backed IBStream for component/controller state.
class HostMemoryStream final : public IBStream {
public:
    explicit HostMemoryStream(std::vector<uint8_t>* storage) : storage_(storage) {
        tuidFromString(iidStringFUnknown(), iidFUnknown_);
        tuidFromString(iidStringIBStream(), iidSelf_);
    }

    tresult queryInterface(const TUID _iid, void** obj) override {
        if (!_iid || !obj) return kInvalidArgument;
        if (tuidEquals(_iid, iidFUnknown_) || tuidEquals(_iid, iidSelf_)) {
            *obj = static_cast<FUnknown*>(this);
            addRef();
            return kResultOk;
        }
        *obj = nullptr;
        return kNoInterface;
    }
    uint32 addRef() override { return refCount_.fetchAdd(1); }
    uint32 release() override {
        const uint32 remaining = refCount_.fetchSub(1);
        if (remaining == 1) delete this;
        return remaining - 1;
    }

    tresult read(void* buffer, int32 numBytes, int32* numBytesRead) override {
        if (!buffer || numBytes < 0) return kInvalidArgument;
        const int64 available =
            static_cast<int64>(storage_->size()) - position_;
        const int64 clamped = numBytes < available ? numBytes
                                                   : (available > 0 ? available : 0);
        const int32 toRead = static_cast<int32>(clamped);
        if (toRead > 0) {
            std::memcpy(buffer, storage_->data() + position_,
                        static_cast<size_t>(toRead));
            position_ += toRead;
        }
        if (numBytesRead) *numBytesRead = toRead;
        return (toRead == numBytes) ? kResultTrue : kResultFalse;
    }

    tresult write(void* buffer, int32 numBytes, int32* numBytesWritten) override {
        if (!buffer || numBytes < 0) return kInvalidArgument;
        const int64 required = position_ + numBytes;
        if (required > static_cast<int64>(storage_->size())) {
            storage_->resize(static_cast<size_t>(required));
        }
        if (numBytes > 0) {
            std::memcpy(storage_->data() + position_, buffer,
                        static_cast<size_t>(numBytes));
            position_ += numBytes;
        }
        if (numBytesWritten) *numBytesWritten = numBytes;
        return kResultTrue;
    }

    tresult seek(int64 pos, int32 mode, int64* result) override {
        int64 target = pos;
        if (mode == kIBSeekCur) {
            target = position_ + pos;
        } else if (mode == kIBSeekEnd) {
            target = static_cast<int64>(storage_->size()) + pos;
        } else if (mode != kIBSeekSet) {
            return kInvalidArgument;
        }
        if (target < 0) return kInvalidArgument;
        position_ = target;
        if (result) *result = position_;
        return kResultTrue;
    }

    tresult tell(int64* pos) override {
        if (!pos) return kInvalidArgument;
        *pos = position_;
        return kResultTrue;
    }

private:
    TUID iidFUnknown_{};
    TUID iidSelf_{};
    std::vector<uint8_t>* storage_ = nullptr;
    int64 position_ = 0;
    AtomicRefCount refCount_;
};

// Parameter change queues (host side IParameterChanges of the SDK).
struct ParamChangePoint {
    int32 sampleOffset = 0;
    ParamValue value = 0.0;
};

class HostParamValueQueue final : public Vst::IParamValueQueue {
public:
    explicit HostParamValueQueue(ParamID id) : id_(id) {
        tuidFromString(iidStringFUnknown(), iidFUnknown_);
        tuidFromString(Vst::iidStringIParamValueQueue(), iidSelf_);
    }

    tresult queryInterface(const TUID _iid, void** obj) override {
        if (!_iid || !obj) return kInvalidArgument;
        if (tuidEquals(_iid, iidFUnknown_) || tuidEquals(_iid, iidSelf_)) {
            *obj = static_cast<FUnknown*>(this);
            addRef();
            return kResultOk;
        }
        *obj = nullptr;
        return kNoInterface;
    }
    uint32 addRef() override { return refCount_.fetchAdd(1); }
    uint32 release() override {
        const uint32 remaining = refCount_.fetchSub(1);
        if (remaining == 1) delete this;
        return remaining - 1;
    }

    ParamID getParameterId() override { return id_; }
    int32 getPointCount() override {
        const size_t count = points_.size();
        return static_cast<int32>(count > 0x7FFFFFFFu ? 0x7FFFFFFFu : count);
    }
    tresult getPoint(int32 index, int32& sampleOffset, ParamValue& value) override {
        if (index < 0 || static_cast<size_t>(index) >= points_.size()) {
            return kInvalidArgument;
        }
        sampleOffset = points_[static_cast<size_t>(index)].sampleOffset;
        value = points_[static_cast<size_t>(index)].value;
        return kResultTrue;
    }
    tresult addPoint(int32 sampleOffset, ParamValue value, int32& index) override {
        points_.push_back(ParamChangePoint{sampleOffset, value});
        index = static_cast<int32>(points_.size() - 1);
        return kResultTrue;
    }

    void clear() { points_.clear(); }

private:
    TUID iidFUnknown_{};
    TUID iidSelf_{};
    ParamID id_ = 0;
    std::vector<ParamChangePoint> points_;
    AtomicRefCount refCount_;
};

class HostParameterChanges final : public Vst::IParameterChanges {
public:
    HostParameterChanges() {
        tuidFromString(iidStringFUnknown(), iidFUnknown_);
        tuidFromString(Vst::iidStringIParameterChanges(), iidSelf_);
    }

    ~HostParameterChanges() { clear(); }
    HostParameterChanges(const HostParameterChanges&) = delete;
    HostParameterChanges& operator=(const HostParameterChanges&) = delete;

    tresult queryInterface(const TUID _iid, void** obj) override {
        if (!_iid || !obj) return kInvalidArgument;
        if (tuidEquals(_iid, iidFUnknown_) || tuidEquals(_iid, iidSelf_)) {
            *obj = static_cast<FUnknown*>(this);
            addRef();
            return kResultOk;
        }
        *obj = nullptr;
        return kNoInterface;
    }
    uint32 addRef() override { return refCount_.fetchAdd(1); }
    uint32 release() override {
        const uint32 remaining = refCount_.fetchSub(1);
        if (remaining == 1) delete this;
        return remaining - 1;
    }

    int32 getParameterCount() override {
        const size_t count = queues_.size();
        return static_cast<int32>(count > 0x7FFFFFFFu ? 0x7FFFFFFFu : count);
    }
    Vst::IParamValueQueue* getParameterData(int32 index) override {
        if (index < 0 || static_cast<size_t>(index) >= queues_.size()) return nullptr;
        return queues_[static_cast<size_t>(index)];
    }
    Vst::IParamValueQueue* addParameterData(const ParamID& id, int32& index) override {
        HostParamValueQueue* queue = new (std::nothrow) HostParamValueQueue(id);
        if (!queue) {
            index = -1;
            return nullptr;
        }
        (void)queue->addRef();  // owned by this list
        queues_.push_back(queue);
        index = static_cast<int32>(queues_.size() - 1);
        return queue;
    }

    void clear() {
        for (auto* queue : queues_) {
            if (queue) queue->release();
        }
        queues_.clear();
    }

private:
    TUID iidFUnknown_{};
    TUID iidSelf_{};
    std::vector<HostParamValueQueue*> queues_;
    AtomicRefCount refCount_;
};

} // namespace

// ---------------------------------------------------------------------------
// VST3Module
// ---------------------------------------------------------------------------
namespace Vst {

namespace {

bool readFactoryError(std::string& out) {
#ifdef _WIN32
    const DWORD code = ::GetLastError();
    if (code == 0) return false;
    char* text = nullptr;
    const DWORD length = ::FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<char*>(&text), 0, nullptr);
    if (length > 0 && text) {
        out.assign(text, length);
        ::LocalFree(text);
        return true;
    }
    return false;
#else
    const char* text = ::dlerror();
    if (text && *text) {
        out.assign(text);
        return true;
    }
    return false;
#endif
}

// A .vst3 file on Windows is usually a bundle directory. Resolve the real
// module binary: Contents/x86_64-win/<stem>.vst3.
std::string resolveVst3Binary(const std::string& pathStd) {
    DWORD attributes = 0;
    (void)attributes;
    bool isDirectory = false;
#ifdef _WIN32
    attributes = ::GetFileAttributesW(std::filesystem::path(pathStd).wstring().c_str());
    isDirectory = (attributes != INVALID_FILE_ATTRIBUTES) &&
                  ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
#else
    std::error_code code;
    isDirectory = std::filesystem::is_directory(pathStd, code);
#endif
    if (!isDirectory) return pathStd;

    const std::filesystem::path root(pathStd);
#ifdef _WIN32
    std::filesystem::path archDir = root / "Contents" / "x86_64-win";
#else
    std::filesystem::path archDir = root / "Contents" / "x86_64-linux";
#endif
    std::error_code code;
    const std::string stem = root.filename().string();
    for (const auto& candidate : {archDir / (stem + ".vst3"), archDir / "module.vst3"}) {
        if (std::filesystem::exists(candidate, code)) {
            return candidate.string();
        }
    }
    if (std::filesystem::exists(archDir, code)) {
        for (const auto& entry : std::filesystem::directory_iterator(archDir, code)) {
            if (entry.is_regular_file(code) &&
                entry.path().extension() == ".vst3") {
                return entry.path().string();
            }
        }
    }
    return pathStd;
}

} // namespace

bool VST3Module::load(const ArtifactCore::String& path) {
    unload();
    const std::string pathStd = ArtifactCore::toStdString(path);
    if (pathStd.empty()) {
        lastError_ = "[VST3] Empty plugin path.";
        return false;
    }

    std::error_code code;
    const std::string binaryPath = resolveVst3Binary(pathStd);
    if (!std::filesystem::exists(binaryPath, code)) {
        lastError_ = "[VST3] File not found: " + binaryPath;
        return false;
    }

#ifdef _WIN32
    moduleHandle_ = ::LoadLibraryW(
        std::filesystem::path(binaryPath).wstring().c_str());
    if (!moduleHandle_) {
        std::string detail;
        if (!readFactoryError(detail)) detail = "unknown error";
        lastError_ = "[VST3] LoadLibrary failed (" + detail + "): " + binaryPath;
        return false;
    }
    getFactoryProc_ = reinterpret_cast<GetFactoryProc>(
        ::GetProcAddress(static_cast<HMODULE>(moduleHandle_), "GetPluginFactory"));
#else
    moduleHandle_ = ::dlopen(binaryPath.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!moduleHandle_) {
        std::string detail;
        if (!readFactoryError(detail)) detail = "unknown error";
        lastError_ = "[VST3] dlopen failed (" + detail + "): " + binaryPath;
        return false;
    }
    getFactoryProc_ = reinterpret_cast<GetFactoryProc>(
        ::dlsym(moduleHandle_, "GetPluginFactory"));
#endif

    if (!getFactoryProc_) {
        lastError_ = "[VST3] GetPluginFactory export not found: " + binaryPath;
        unload();
        return false;
    }

    factory_ = getFactoryProc_();
    if (!factory_) {
        lastError_ = "[VST3] GetPluginFactory returned null: " + binaryPath;
        unload();
        return false;
    }
    return true;
}

void VST3Module::unload() {
    if (factory_) {
        factory_->release();
        factory_ = nullptr;
    }
    getFactoryProc_ = nullptr;

    if (moduleHandle_) {
#ifdef _WIN32
        ::FreeLibrary(static_cast<HMODULE>(moduleHandle_));
#else
        ::dlclose(moduleHandle_);
#endif
        moduleHandle_ = nullptr;
    }
}

int32 VST3Module::classCount() const {
    if (!factory_) return 0;
    // A negative or absurd count means the factory is not usable.
    const int32 count = factory_->countClasses();
    return (count >= 0 && count <= 0x7FFFFFFF) ? count : 0;
}

bool VST3Module::classInfo(int32 index, PClassInfo& out) const {
    std::memset(&out, 0, sizeof(out));
    if (!factory_ || index < 0 || index >= classCount()) return false;
    if (factory_->getClassInfo(index, &out) != kResultOk) {
        std::memset(&out, 0, sizeof(out));
        return false;
    }
    // cid is a binary 16 byte GUID, never a string: keep all 16 bytes.
    // category/name follow the SDK strncpy8 convention (NUL padded);
    // force the NUL in case a foreign factory writes a full buffer.
    out.category[PClassInfo::kCategorySize - 1] = '\0';
    out.name[PClassInfo::kNameSize - 1] = '\0';
    return true;
}

void* VST3Module::createInstance(FIDString cid, FIDString iid) const {
    if (!factory_ || !cid || !iid) return nullptr;
    void* object = nullptr;
    if (factory_->createInstance(cid, iid, &object) != kResultOk) return nullptr;
    return object;
}

// ---------------------------------------------------------------------------
// VST3EffectHost
// ---------------------------------------------------------------------------
class VST3EffectHost::Impl {
public:
    VST3Module module;
    Vst::IComponent* component = nullptr;
    Vst::IAudioProcessor* processor = nullptr;
    Vst::IEditController* controller = nullptr;
    HostApplication* host = nullptr;

    Steinberg::PFactoryInfo factoryInfo{};
    Steinberg::PClassInfo classInfo{};
    std::string className_;
    std::string vendorName_;

    int32 audioInputBuses = 0;
    int32 audioOutputBuses = 0;
    int32 inputChannels = 0;
    int32 outputChannels = 0;
    std::string lastError_;

    bool active = false;
    bool processing = false;
    bool doublePrecision = false;
    double sampleRate = 48000.0;
    int32 maxBlockSize = 512;

    // Per block working set, preallocated in setup(). The process call itself
    // performs no allocation; the pending parameter list is drained, not
    // cleared with ownership churn, so its capacity is kept.
    std::vector<Vst::AudioBusBuffers> inputBuses;
    std::vector<Vst::AudioBusBuffers> outputBuses;
    std::vector<Sample32*> inputPtrs32;
    std::vector<Sample32*> outputPtrs32;
    std::vector<Sample64*> inputPtrs64;
    std::vector<Sample64*> outputPtrs64;
    Vst::ProcessData processData{};
    HostParameterChanges* inputParameterChanges = nullptr;
    std::vector<std::pair<ParamID, ParamValue>> pendingParameters;

    Impl() {
        host = new (std::nothrow) HostApplication();
        inputParameterChanges = new (std::nothrow) HostParameterChanges();
    }

    ~Impl() { shutdown(); }

    void shutdown() {
        if (processor && processing) {
            processor->setProcessing(0);
            processing = false;
        }
        if (component && active) {
            component->setActive(0);
            active = false;
        }
        if (controller) {
            controller->terminate();
            controller->release();
            controller = nullptr;
        }
        if (component) {
            component->terminate();
            component->release();
            component = nullptr;
        }
        processor = nullptr;
        if (inputParameterChanges) {
            inputParameterChanges->release();
            inputParameterChanges = nullptr;
        }
        if (host) {
            host->release();
            host = nullptr;
        }
        module.unload();
        clearWorkingSet();
    }

    void clearWorkingSet() {
        inputBuses.clear();
        outputBuses.clear();
        inputPtrs32.clear();
        outputPtrs32.clear();
        inputPtrs64.clear();
        outputPtrs64.clear();
        processData = Vst::ProcessData{};
        pendingParameters.clear();
    }

    void fail(const std::string& message) { lastError_ = message; }
};

VST3EffectHost::VST3EffectHost() : impl_(new (std::nothrow) Impl()) {}

VST3EffectHost::~VST3EffectHost() {
    delete impl_;
    impl_ = nullptr;
}

void VST3EffectHost::unload() {
    if (!impl_) return;
    impl_->shutdown();
    delete impl_;
    impl_ = new (std::nothrow) Impl();
}

bool VST3EffectHost::isLoaded() const {
    return impl_ && impl_->component && impl_->processor;
}

const char8* VST3EffectHost::lastError() const {
    return impl_ ? impl_->lastError_.c_str() : "";
}

bool VST3EffectHost::load(const ArtifactCore::String& pluginPath, int32 classIndex) {
    unload();
    if (!impl_ || !impl_->host || !impl_->inputParameterChanges) {
        return false;
    }

    if (!impl_->module.load(pluginPath)) {
        if (impl_) impl_->fail(impl_->module.lastError());
        return false;
    }

    IPluginFactory* factory = impl_->module.getFactory();
    if (!factory) {
        impl_->fail("[VST3] No factory available.");
        unload();
        return false;
    }

    const int32 classCount = impl_->module.classCount();
    if (classCount <= 0) {
        impl_->fail("[VST3] Factory exports no classes.");
        unload();
        return false;
    }

    // Pick the requested class, otherwise the first "Audio Module Class".
    int32 selected = -1;
    PClassInfo selectedInfo{};
    if (classIndex >= 0) {
        if (classIndex < classCount && impl_->module.classInfo(classIndex, selectedInfo)) {
            selected = classIndex;
        }
    } else {
        PClassInfo candidate{};
        for (int32 i = 0; i < classCount; ++i) {
            if (!impl_->module.classInfo(i, candidate)) continue;
            if (std::strcmp(candidate.category, Vst::kVstAudioEffectClass) == 0) {
                selected = i;
                selectedInfo = candidate;
                break;
            }
        }
    }
    if (selected < 0) {
        impl_->fail("[VST3] No audio effect class found in module.");
        unload();
        return false;
    }

    impl_->classInfo = selectedInfo;
    impl_->className_ = boundedCString(selectedInfo.name, PClassInfo::kNameSize);
    PFactoryInfo factoryInfo{};
    if (factory->getFactoryInfo(&factoryInfo) == kResultOk) {
        impl_->factoryInfo = factoryInfo;
        impl_->factoryInfo.vendor[PFactoryInfo::kNameSize - 1] = '\0';
        impl_->vendorName_ =
            boundedCString(impl_->factoryInfo.vendor, PFactoryInfo::kNameSize);
    }

    char8 cidText[33];
    tuidToString(selectedInfo.cid, cidText);

    // The component must expose IComponent; the audio processor is obtained
    // from the same object (components implement both).
    void* componentObject =
        impl_->module.createInstance(cidText, Vst::iidStringIComponent());
    if (!componentObject) {
        impl_->fail("[VST3] Could not instantiate the audio effect class.");
        unload();
        return false;
    }
    auto* component = static_cast<Vst::IComponent*>(componentObject);
    if (component->initialize(impl_->host) != kResultOk) {
        component->release();
        impl_->fail("[VST3] Component refused to initialize.");
        unload();
        return false;
    }

    void* processorObject = nullptr;
    TUID processorIid{};
    tuidFromString(Vst::iidStringIAudioProcessor(), processorIid);
    if (component->queryInterface(processorIid, &processorObject) != kResultOk ||
        !processorObject) {
        component->terminate();
        component->release();
        impl_->fail("[VST3] Component does not expose IAudioProcessor.");
        unload();
        return false;
    }

    impl_->component = component;
    impl_->processor = static_cast<Vst::IAudioProcessor*>(processorObject);

    // Controller: either a dedicated class or part of the component itself.
    TUID controllerCid{};
    if (component->getControllerClassId(controllerCid) == kResultOk) {
        char8 controllerCidText[33];
        tuidToString(controllerCid, controllerCidText);
        void* controllerObject = impl_->module.createInstance(
            controllerCidText, Vst::iidStringIEditController());
        if (controllerObject) {
            auto* controller =
                static_cast<Vst::IEditController*>(controllerObject);
            if (controller->initialize(impl_->host) == kResultOk) {
                impl_->controller = controller;
            } else {
                controller->release();
            }
        }
    }
    if (!impl_->controller) {
        // Single object layout: the component may implement the controller.
        TUID editCid{};
        tuidFromString(Vst::iidStringIEditController(), editCid);
        void* sameObject = nullptr;
        if (component->queryInterface(editCid, &sameObject) == kResultOk &&
            sameObject) {
            impl_->controller = static_cast<Vst::IEditController*>(sameObject);
        }
    }

    // Read the audio bus layout; default-activate the audio buses.
    impl_->audioInputBuses = component->getBusCount(Vst::kAudio, Vst::kInput);
    impl_->audioOutputBuses = component->getBusCount(Vst::kAudio, Vst::kOutput);
    impl_->inputChannels = 0;
    impl_->outputChannels = 0;
    for (int32 i = 0; i < impl_->audioInputBuses; ++i) {
        Vst::BusInfo info{};
        if (component->getBusInfo(Vst::kAudio, Vst::kInput, i, info) != kResultOk) {
            continue;
        }
        (void)component->activateBus(Vst::kAudio, Vst::kInput, i, 1);
        impl_->inputChannels += info.channelCount > 0 ? info.channelCount : 0;
    }
    for (int32 i = 0; i < impl_->audioOutputBuses; ++i) {
        Vst::BusInfo info{};
        if (component->getBusInfo(Vst::kAudio, Vst::kOutput, i, info) != kResultOk) {
            continue;
        }
        (void)component->activateBus(Vst::kAudio, Vst::kOutput, i, 1);
        impl_->outputChannels += info.channelCount > 0 ? info.channelCount : 0;
    }
    return true;
}

const char8* VST3EffectHost::className() const {
    return impl_ ? impl_->className_.c_str() : "";
}
const char8* VST3EffectHost::vendorName() const {
    return impl_ ? impl_->vendorName_.c_str() : "";
}
int32 VST3EffectHost::inputBusCount() const {
    return impl_ ? impl_->audioInputBuses : 0;
}
int32 VST3EffectHost::outputBusCount() const {
    return impl_ ? impl_->audioOutputBuses : 0;
}
int32 VST3EffectHost::inputChannelCount() const {
    return impl_ ? impl_->inputChannels : 0;
}
int32 VST3EffectHost::outputChannelCount() const {
    return impl_ ? impl_->outputChannels : 0;
}
int32 VST3EffectHost::parameterCount() const {
    if (!impl_ || !impl_->controller) return 0;
    const int32 count = impl_->controller->getParameterCount();
    return count >= 0 ? count : 0;
}

bool VST3EffectHost::getParameterInfo(int32 index, Vst::ParameterInfo& info) const {
    std::memset(&info, 0, sizeof(info));
    if (!impl_ || !impl_->controller || index < 0) return false;
    if (impl_->controller->getParameterInfo(index, info) != kResultOk) {
        std::memset(&info, 0, sizeof(info));
        return false;
    }
    info.title[127] = 0;  // String128 has 128 UTF-16 code units
    return true;
}

bool VST3EffectHost::getParameterDisplay(int32 index, String128 text) const {
    if (!text) return false;
    std::memset(text, 0, sizeof(String128));
    if (!impl_ || !impl_->controller || index < 0) return false;
    Vst::ParameterInfo info{};
    if (!getParameterInfo(index, info)) return false;
    const ParamValue value = parameterNormalized(info.id);
    return impl_->controller->getParamStringByValue(info.id, value, text) == kResultOk;
}

bool VST3EffectHost::processFloat(Sample32* const* inputs, int32 numInputChannels,
                                Sample32* const* outputs, int32 numOutputChannels,
                                int32 numSamples) {
    if (!impl_ || !isLoaded() || !impl_->processing || numSamples <= 0) {
        return false;
    }
    if (impl_->doublePrecision) return false;
    if (!inputs || !outputs) return false;
    if (numInputChannels < impl_->inputChannels ||
        numOutputChannels < impl_->outputChannels) {
        return false;
    }
    if (!flushPendingParameters()) return false;

    // Distribute the caller channel pointers over the preallocated buses.
    int32 inputCursor = 0;
    for (int32 i = 0; i < impl_->audioInputBuses; ++i) {
        Vst::BusInfo info{};
        if (impl_->component->getBusInfo(Vst::kAudio, Vst::kInput, i, info) != kResultOk ||
            info.channelCount <= 0) {
            impl_->inputBuses[static_cast<size_t>(i)].numChannels = 0;
            impl_->inputBuses[static_cast<size_t>(i)].channelBuffers32 = nullptr;
            continue;
        }
        impl_->inputBuses[static_cast<size_t>(i)].numChannels = info.channelCount;
        for (int32 c = 0; c < info.channelCount; ++c) {
            impl_->inputPtrs32[static_cast<size_t>(inputCursor + c)] =
                inputs[inputCursor + c];
        }
        impl_->inputBuses[static_cast<size_t>(i)].channelBuffers32 =
            impl_->inputPtrs32.data() + inputCursor;
        inputCursor += info.channelCount;
    }
    int32 outputCursor = 0;
    for (int32 i = 0; i < impl_->audioOutputBuses; ++i) {
        Vst::BusInfo info{};
        if (impl_->component->getBusInfo(Vst::kAudio, Vst::kOutput, i, info) != kResultOk ||
            info.channelCount <= 0) {
            impl_->outputBuses[static_cast<size_t>(i)].numChannels = 0;
            impl_->outputBuses[static_cast<size_t>(i)].channelBuffers32 = nullptr;
            continue;
        }
        impl_->outputBuses[static_cast<size_t>(i)].numChannels = info.channelCount;
        for (int32 c = 0; c < info.channelCount; ++c) {
            impl_->outputPtrs32[static_cast<size_t>(outputCursor + c)] =
                outputs[outputCursor + c];
        }
        impl_->outputBuses[static_cast<size_t>(i)].channelBuffers32 =
            impl_->outputPtrs32.data() + outputCursor;
        outputCursor += info.channelCount;
    }

    impl_->processData.numSamples = numSamples;
    return impl_->processor->process(impl_->processData) == kResultOk;
}

bool VST3EffectHost::processDouble(Sample64* const* inputs, int32 numInputChannels,
                                   Sample64* const* outputs, int32 numOutputChannels,
                                   int32 numSamples) {
    if (!impl_ || !isLoaded() || !impl_->processing || numSamples <= 0) {
        return false;
    }
    if (!impl_->doublePrecision) return false;
    if (!inputs || !outputs) return false;
    if (numInputChannels < impl_->inputChannels ||
        numOutputChannels < impl_->outputChannels) {
        return false;
    }
    if (!flushPendingParameters()) return false;

    int32 inputCursor = 0;
    for (int32 i = 0; i < impl_->audioInputBuses; ++i) {
        Vst::BusInfo info{};
        if (impl_->component->getBusInfo(Vst::kAudio, Vst::kInput, i, info) != kResultOk ||
            info.channelCount <= 0) {
            impl_->inputBuses[static_cast<size_t>(i)].numChannels = 0;
            impl_->inputBuses[static_cast<size_t>(i)].channelBuffers64 = nullptr;
            continue;
        }
        impl_->inputBuses[static_cast<size_t>(i)].numChannels = info.channelCount;
        for (int32 c = 0; c < info.channelCount; ++c) {
            impl_->inputPtrs64[static_cast<size_t>(inputCursor + c)] =
                inputs[inputCursor + c];
        }
        impl_->inputBuses[static_cast<size_t>(i)].channelBuffers64 =
            impl_->inputPtrs64.data() + inputCursor;
        inputCursor += info.channelCount;
    }
    int32 outputCursor = 0;
    for (int32 i = 0; i < impl_->audioOutputBuses; ++i) {
        Vst::BusInfo info{};
        if (impl_->component->getBusInfo(Vst::kAudio, Vst::kOutput, i, info) != kResultOk ||
            info.channelCount <= 0) {
            impl_->outputBuses[static_cast<size_t>(i)].numChannels = 0;
            impl_->outputBuses[static_cast<size_t>(i)].channelBuffers64 = nullptr;
            continue;
        }
        impl_->outputBuses[static_cast<size_t>(i)].numChannels = info.channelCount;
        for (int32 c = 0; c < info.channelCount; ++c) {
            impl_->outputPtrs64[static_cast<size_t>(outputCursor + c)] =
                outputs[outputCursor + c];
        }
        impl_->outputBuses[static_cast<size_t>(i)].channelBuffers64 =
            impl_->outputPtrs64.data() + outputCursor;
        outputCursor += info.channelCount;
    }

    impl_->processData.numSamples = numSamples;
    return impl_->processor->process(impl_->processData) == kResultOk;
}



ParamValue VST3EffectHost::parameterNormalized(ParamID id) const {
    if (!impl_ || !impl_->controller) return 0.0;
    return impl_->controller->getParamNormalized(id);
}

bool VST3EffectHost::setParameterNormalized(ParamID id, ParamValue value) {
    if (!impl_ || !isLoaded()) return false;
    const ParamValue clamped = value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
    if (impl_->controller) {
        if (impl_->controller->setParamNormalized(id, clamped) != kResultOk) {
            return false;
        }
    }
    // Record for the next block; the list is drained, not reallocated.
    impl_->pendingParameters.emplace_back(id, clamped);
    return true;
}

uint32 VST3EffectHost::latencySamples() const {
    if (!impl_ || !impl_->processor) return 0;
    return impl_->processor->getLatencySamples();
}
uint32 VST3EffectHost::tailSamples() const {
    if (!impl_ || !impl_->processor) return 0;
    return impl_->processor->getTailSamples();
}
bool VST3EffectHost::isActive() const {
    return impl_ ? impl_->active : false;
}
bool VST3EffectHost::isProcessing() const {
    return impl_ ? impl_->processing : false;
}

bool VST3EffectHost::setup(double sampleRate, int32 maxBlockSize, bool doublePrecision) {
    if (!impl_ || !isLoaded()) return false;
    if (sampleRate <= 0.0 || sampleRate > 1000000.0) return false;
    if (maxBlockSize <= 0 || maxBlockSize > 65536) return false;

    constexpr SpeakerArrangement kSpeakerStereo = 0x3u;  // vstspeaker.h kStereo
    (void)kSpeakerStereo;

    impl_->doublePrecision = doublePrecision;
    impl_->sampleRate = sampleRate;
    impl_->maxBlockSize = maxBlockSize;
    const int32 sampleSize = doublePrecision ? Vst::kSample64 : Vst::kSample32;
    if (impl_->processor->canProcessSampleSize(sampleSize) != kResultTrue) {
        if (doublePrecision) {
            impl_->fail("[VST3] Plug-in cannot process 64-bit float blocks.");
            return false;
        }
        // Even 32-bit precision was refused: continue anyway, the plug-in
        // may still accept it in setupProcessing.
    }

    // Ask for a simple arrangement; fall back to the plug-in defaults.
    std::vector<SpeakerArrangement> inArr;
    if (impl_->audioInputBuses > 0) {
        inArr.assign(static_cast<size_t>(impl_->audioInputBuses), kSpeakerStereo);
    }
    std::vector<SpeakerArrangement> outArr;
    if (impl_->audioOutputBuses > 0) {
        outArr.assign(static_cast<size_t>(impl_->audioOutputBuses), kSpeakerStereo);
    }
    (void)impl_->processor->setBusArrangements(inArr.data(), impl_->audioInputBuses,
                                               outArr.data(), impl_->audioOutputBuses);

    // Re-read the effective bus layout.
    impl_->inputChannels = 0;
    impl_->outputChannels = 0;
    for (int32 i = 0; i < impl_->audioInputBuses; ++i) {
        Vst::BusInfo info{};
        if (impl_->component->getBusInfo(Vst::kAudio, Vst::kInput, i, info) == kResultOk &&
            info.channelCount > 0) {
            impl_->inputChannels += info.channelCount;
        }
    }
    for (int32 i = 0; i < impl_->audioOutputBuses; ++i) {
        Vst::BusInfo info{};
        if (impl_->component->getBusInfo(Vst::kAudio, Vst::kOutput, i, info) == kResultOk &&
            info.channelCount > 0) {
            impl_->outputChannels += info.channelCount;
        }
    }

    Vst::ProcessSetup vstSetup{};
    vstSetup.processMode = Vst::kRealtime;
    vstSetup.symbolicSampleSize = sampleSize;
    vstSetup.maxSamplesPerBlock = maxBlockSize;
    vstSetup.sampleRate = sampleRate;
    if (impl_->processor->setupProcessing(vstSetup) != kResultOk) {
        impl_->fail("[VST3] setupProcessing was refused.");
        return false;
    }
    (void)impl_->component->setActive(1);
    impl_->active = true;

    // Preallocate the per block working set once.
    impl_->inputBuses.assign(static_cast<size_t>(impl_->audioInputBuses),
                             Vst::AudioBusBuffers{});
    impl_->outputBuses.assign(static_cast<size_t>(impl_->audioOutputBuses),
                              Vst::AudioBusBuffers{});
    impl_->inputPtrs32.assign(static_cast<size_t>(impl_->inputChannels), nullptr);
    impl_->outputPtrs32.assign(static_cast<size_t>(impl_->outputChannels), nullptr);
    impl_->inputPtrs64.assign(static_cast<size_t>(impl_->inputChannels), nullptr);
    impl_->outputPtrs64.assign(static_cast<size_t>(impl_->outputChannels), nullptr);

    impl_->processData = Vst::ProcessData{};
    impl_->processData.processMode = Vst::kRealtime;
    impl_->processData.symbolicSampleSize = sampleSize;
    impl_->processData.numInputs = impl_->audioInputBuses;
    impl_->processData.numOutputs = impl_->audioOutputBuses;
    impl_->processData.inputs =
        impl_->inputBuses.empty() ? nullptr : impl_->inputBuses.data();
    impl_->processData.outputs =
        impl_->outputBuses.empty() ? nullptr : impl_->outputBuses.data();
    impl_->processData.inputParameterChanges = impl_->inputParameterChanges;
    return true;
}

bool VST3EffectHost::setActive(bool active) {
    if (!impl_ || !isLoaded()) return false;
    if (impl_->component->setActive(active ? 1 : 0) != kResultOk) return false;
    impl_->active = active;
    return true;
}

bool VST3EffectHost::setProcessing(bool processing) {
    if (!impl_ || !isLoaded() || !impl_->active) return false;
    if (impl_->processor->setProcessing(processing ? 1 : 0) != kResultOk) {
        return false;
    }
    impl_->processing = processing;
    return true;
}

bool VST3EffectHost::flushPendingParameters() {
    if (!impl_ || !impl_->inputParameterChanges) return false;
    impl_->inputParameterChanges->clear();
    for (const auto& [id, value] : impl_->pendingParameters) {
        int32 index = -1;
        Vst::IParamValueQueue* queue =
            impl_->inputParameterChanges->addParameterData(id, index);
        if (!queue) return false;
        int32 point = -1;
        if (queue->addPoint(0, value, point) != kResultOk) return false;
    }
    impl_->pendingParameters.clear();
    return true;
}

bool VST3EffectHost::saveState(std::vector<uint8_t>& outState) const {
    outState.clear();
    if (!impl_ || !isLoaded()) return false;

    std::vector<uint8_t> componentState;
    {
        HostMemoryStream* stream = new (std::nothrow) HostMemoryStream(&componentState);
        if (!stream) return false;
        (void)stream->addRef();
        if (impl_->component->getState(stream) != kResultOk) {
            stream->release();
            return false;
        }
        stream->release();
    }

    std::vector<uint8_t> controllerState;
    if (impl_->controller) {
        HostMemoryStream* stream =
            new (std::nothrow) HostMemoryStream(&controllerState);
        if (!stream) return false;
        (void)stream->addRef();
        if (impl_->controller->getState(stream) != kResultOk) {
            stream->release();
            return false;
        }
        stream->release();
    }

    const size_t componentLength = std::min<size_t>(componentState.size(), 0xFFFFFFFFu);
    const size_t controllerLength =
        std::min<size_t>(controllerState.size(), 0xFFFFFFFFu);
    outState.reserve(12 + componentLength + controllerLength);
    auto appendU32 = [&](uint32 value) {
        outState.push_back(static_cast<uint8_t>(value & 0xFFu));
        outState.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
        outState.push_back(static_cast<uint8_t>((value >> 16) & 0xFFu));
        outState.push_back(static_cast<uint8_t>((value >> 24) & 0xFFu));
    };
    appendU32(0x56535433u);                                  // "VST3"
    appendU32(static_cast<uint32>(componentLength));
    appendU32(static_cast<uint32>(controllerLength));
    outState.insert(outState.end(), componentState.begin(),
                    componentState.begin() + componentLength);
    outState.insert(outState.end(), controllerState.begin(),
                    controllerState.begin() + controllerLength);
    return true;
}

bool VST3EffectHost::loadState(const std::vector<uint8_t>& state) {
    if (!impl_ || !isLoaded() || state.size() < 12) return false;
    if (state[0] != 'V' || state[1] != 'S' || state[2] != 'T' || state[3] != '3') {
        return false;
    }
    auto readU32 = [&](size_t offset) -> uint32 {
        return static_cast<uint32>(state[offset]) |
               (static_cast<uint32>(state[offset + 1]) << 8) |
               (static_cast<uint32>(state[offset + 2]) << 16) |
               (static_cast<uint32>(state[offset + 3]) << 24);
    };
    const uint32 componentSize = readU32(4);
    const uint32 controllerSize = readU32(8);
    if (static_cast<uint64>(componentSize) + controllerSize + 12 > state.size()) {
        return false;
    }

    std::vector<uint8_t> componentState(
        state.begin() + 12, state.begin() + 12 + componentSize);
    {
        HostMemoryStream* stream = new (std::nothrow) HostMemoryStream(&componentState);
        if (!stream) return false;
        (void)stream->addRef();
        (void)impl_->component->setState(stream);
        if (impl_->controller) {
            (void)impl_->controller->setComponentState(stream);
        }
        stream->release();
    }
    if (controllerSize > 0 && impl_->controller) {
        std::vector<uint8_t> controllerState(
            state.begin() + 12 + componentSize,
            state.begin() + 12 + componentSize + controllerSize);
        HostMemoryStream* stream =
            new (std::nothrow) HostMemoryStream(&controllerState);
        if (!stream) return false;
        (void)stream->addRef();
        (void)impl_->controller->setState(stream);
        stream->release();
    }
    return true;
}

// __VST3_LOADER_APPEND10__
} // namespace Vst
} // namespace Steinberg
