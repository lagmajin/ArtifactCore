module;
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <dlfcn.h>
#endif
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <cmath>
#include <utility>
#include <algorithm>
#include <memory>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>
#include <filesystem>

module CLAP.Host;

import Memory.SharedPtr;
import Container.NamedVector;

namespace clap {

namespace fs = std::filesystem;

const void* hostGetExtension(const clap_host*, const char*) {
    return nullptr;
}

void hostRequestRestart(const clap_host*) {}
void hostRequestProcess(const clap_host*) {}
void hostRequestCallback(const clap_host*) {}

// ─────────────────────────────────────────────────────────
// PluginLibrary — DLLラッパー、clap_entry を解決
// ─────────────────────────────────────────────────────────
struct PluginLibrary {
    void* handle = nullptr;
    std::string path;
    const clap_plugin_entry* entry = nullptr;

    ~PluginLibrary() {
        if (handle) {
            if (entry) entry->deinit();
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(handle));
#else
            dlclose(handle);
#endif
        }
    }

    bool load(const std::string& path) {
#ifdef _WIN32
        handle = LoadLibraryW(fs::path(path).wstring().c_str());
        if (!handle) return false;
        auto sym = GetProcAddress(static_cast<HMODULE>(handle), "clap_entry");
#else
        handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle) return false;
        auto sym = dlsym(handle, "clap_entry");
#endif
        if (!sym) {
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(handle));
#else
            dlclose(handle);
#endif
            handle = nullptr;
            return false;
        }
        // CLAP entry は DLL から構造体オブジェクトとしてエクスポートされる。
        // GetProcAddress の戻り値は Windows の関数ポインタ型なので、オブジェクトの
        // アドレスへ reinterpret_cast する。
        entry = reinterpret_cast<const clap_plugin_entry*>(sym);
        if (!entry || !entry->init || !entry->deinit ||
            !entry->get_plugin_count || !entry->get_plugin_descriptor ||
            !entry->create_plugin) {
            entry = nullptr;
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(handle));
#else
            dlclose(handle);
#endif
            handle = nullptr;
            return false;
        }
        if (!entry->init(path.c_str())) {
            entry = nullptr;
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(handle));
#else
            dlclose(handle);
#endif
            handle = nullptr;
            return false;
        }
        return true;
    }
};

// ─────────────────────────────────────────────────────────
// PluginDescriptor 変換: clap_plugin_descriptor → clap::PluginDescriptor
// ─────────────────────────────────────────────────────────
static PluginDescriptor makeDescriptor(const clap_plugin_descriptor* cd) {
    PluginDescriptor pd;
    if (cd->id) pd.id = cd->id;
    if (cd->name) pd.name = cd->name;
    if (cd->vendor) pd.vendor = cd->vendor;
    if (cd->url) pd.url = cd->url;
    if (cd->manual_url) pd.manualUrl = cd->manual_url;
    if (cd->support_url) pd.supportUrl = cd->support_url;
    if (cd->version) pd.version = cd->version;
    if (cd->description) pd.description = cd->description;
    if (cd->features) {
        constexpr std::size_t kMaxFeatures = 256;
        std::size_t featureCount = 0;
        for (const char** f = cd->features;
             *f && featureCount < kMaxFeatures; ++f, ++featureCount) {
            pd.features.emplace_back(*f);
        }
    }
    return pd;
}

struct PendingInputContext {
    const std::vector<clap_event_param_value>* events = nullptr;
};

uint32_t pendingInputSize(const clap_input_events* list) {
    const auto* context = static_cast<const PendingInputContext*>(list->context);
    return context && context->events ? static_cast<uint32_t>(context->events->size()) : 0;
}

const clap_event_header* pendingInputGet(const clap_input_events* list, uint32_t index) {
    const auto* context = static_cast<const PendingInputContext*>(list->context);
    if (!context || !context->events || index >= context->events->size()) return nullptr;
    return &context->events->at(index).header;
}

// ─────────────────────────────────────────────────────────
// PluginInstance — const clap_plugin* の具象ラッパー
// ─────────────────────────────────────────────────────────
PluginInstance::PluginInstance(const clap_plugin* plugin,
                               const PluginDescriptor& desc,
                               const clap_plugin_entry* entry)
    : plugin_(plugin), desc_(desc), entry_(entry) {}

PluginInstance::~PluginInstance() {
    if (processing_) stopProcessing();
    if (active_) deactivate();
    if (plugin_ && plugin_->destroy)
        plugin_->destroy(plugin_);
    plugin_ = nullptr;
    paramExt_ = nullptr;
    pendingParams_.clear();
}

bool PluginInstance::init() {
    return plugin_ && plugin_->init && plugin_->init(plugin_);
}

void PluginInstance::destroy() {
    if (processing_) stopProcessing();
    if (active_) deactivate();
    if (plugin_ && plugin_->destroy)
        plugin_->destroy(plugin_);
    plugin_ = nullptr;
    paramExt_ = nullptr;
    pendingParams_.clear();
}

bool PluginInstance::activate(float64 sampleRate, uint32 minFrameCount, uint32 maxFrameCount) {
    if (!plugin_ || !plugin_->activate || active_) return false;
    active_ = plugin_->activate(plugin_, sampleRate, minFrameCount, maxFrameCount);
    return active_;
}

void PluginInstance::deactivate() {
    if (active_ && plugin_ && plugin_->deactivate)
        plugin_->deactivate(plugin_);
    active_ = false;
}

bool PluginInstance::startProcessing() {
    if (!active_ || processing_ || !plugin_ || !plugin_->start_processing) return false;
    processing_ = plugin_->start_processing(plugin_);
    return processing_;
}

void PluginInstance::stopProcessing() {
    if (processing_ && plugin_ && plugin_->stop_processing)
        plugin_->stop_processing(plugin_);
    processing_ = false;
}

bool PluginInstance::process(const Process& process) {
    if (!plugin_ || !plugin_->process || !active_ || !processing_ ||
        process.framesCount == 0 || process.framesCount > kProcessMaxFrames) return false;
    clap_process cp{};
    cp.frames_count = process.framesCount;
    cp.frame_offset = process.frameIndex;
    // AudioBuffer → deinterleaved float*
    if (process.audioInputs && process.audioInputsCount > 0) {
        const AudioBuffer& input = process.audioInputs[0];
        cp.audio_inputs_count = std::min<uint32>(input.channelCount, 2);
        for (uint32 channel = 0; channel < cp.audio_inputs_count; ++channel) {
            cp.audio_inputs[channel] = input.channels[channel]
                ? input.channels[channel] : input.data32;
        }
    }
    if (process.audioOutputs && process.audioOutputsCount > 0) {
        const AudioBuffer& output = process.audioOutputs[0];
        cp.audio_outputs_count = std::min<uint32>(output.channelCount, 2);
        for (uint32 channel = 0; channel < cp.audio_outputs_count; ++channel) {
            cp.audio_outputs[channel] = output.channels[channel]
                ? output.channels[channel] : output.data32;
        }
    }
    std::vector<clap_event_param_value> parameterEvents;
    parameterEvents.reserve(pendingParams_.size());
    for (const auto& pending : pendingParams_) {
        clap_event_param_value event{};
        event.header.size = sizeof(clap_event_param_value);
        event.header.time = 0;
        event.header.space_id = 0;
        event.header.type = 6; // CLAP_EVENT_PARAM_VALUE
        event.param_id = pending.id;
        event.note_id = -1;
        event.port_index = -1;
        event.channel = -1;
        event.key = -1;
        event.value = pending.value;
        parameterEvents.push_back(event);
    }
    PendingInputContext inputContext{&parameterEvents};
    clap_input_events inputEvents{&inputContext, pendingInputSize, pendingInputGet};
    cp.reserved[0] = &inputEvents;
    const bool processed = plugin_->process(plugin_, &cp);
    pendingParams_.clear();
    return processed;
}

const void* PluginInstance::getExtension(const char* id) {
    return plugin_ && plugin_->get_extension
        ? plugin_->get_extension(plugin_, id) : nullptr;
}

bool PluginInstance::resolveParamExt() {
    if (paramExt_) return true;
    paramExt_ = const_cast<void*>(getExtension("clap.params"));
    return paramExt_ != nullptr;
}

uint32 PluginInstance::paramsCount() const {
    auto* self = const_cast<PluginInstance*>(this);
    if (!self->resolveParamExt()) return 0;
    const auto* extension = static_cast<const clap_plugin_params*>(self->paramExt_);
    constexpr uint32 kMaxParameters = 65536;
    const uint32 count = extension->count ? extension->count(plugin_) : 0;
    return std::min(count, kMaxParameters);
}

bool PluginInstance::paramInfo(uint32 index, void* info) const {
    if (!info) return false;
    if (index >= paramsCount()) return false;
    auto* self = const_cast<PluginInstance*>(this);
    if (!self->resolveParamExt()) return false;
    const auto* extension = static_cast<const clap_plugin_params*>(self->paramExt_);
    return extension->get_info && extension->get_info(plugin_, index,
                                                       static_cast<clap_param_info*>(info));
}

double PluginInstance::paramValue(uint32 paramId) const {
    auto* self = const_cast<PluginInstance*>(this);
    if (!self->resolveParamExt()) return 0.0;
    const auto* extension = static_cast<const clap_plugin_params*>(self->paramExt_);
    double value = 0.0;
    return extension->get_value && extension->get_value(plugin_, paramId, &value)
        ? value : 0.0;
}

bool PluginInstance::paramSetValue(uint32 paramId, double value) {
    if (!resolveParamExt() || !std::isfinite(value)) return false;
    ParamInfo info;
    bool found = false;
    for (uint32 index = 0; index < paramsCount(); ++index) {
        if (paramGetInfo(index, info) && info.id == paramId) {
            found = true;
            break;
        }
    }
    if (!found || !std::isfinite(info.minValue) || !std::isfinite(info.maxValue) ||
        info.minValue > info.maxValue) {
        return false;
    }
    pendingParams_.push_back({paramId, std::clamp(value, info.minValue, info.maxValue)});
    return true;
}

bool PluginInstance::paramGetDisplay(uint32 paramId, char* buf, uint32 size) const {
    if (!buf || size == 0) return false;
    buf[0] = '\0';
    ParamInfo info;
    bool found = false;
    for (uint32 index = 0; index < paramsCount(); ++index) {
        if (paramGetInfo(index, info) && info.id == paramId) {
            found = true;
            break;
        }
    }
    if (!found) return false;
    auto* self = const_cast<PluginInstance*>(this);
    if (!self->resolveParamExt()) return false;
    const auto* extension = static_cast<const clap_plugin_params*>(self->paramExt_);
    const bool ok = extension->value_to_text && extension->value_to_text(
        plugin_, paramId, paramValue(paramId), buf, size);
    buf[size - 1] = '\0';
    return ok;
}

bool PluginInstance::paramGetInfo(uint32 index, ParamInfo& info) const {
    clap_param_info clapInfo{};
    if (!paramInfo(index, &clapInfo)) return false;
    if (!std::isfinite(clapInfo.min_value) || !std::isfinite(clapInfo.max_value) ||
        !std::isfinite(clapInfo.default_value) ||
        clapInfo.min_value > clapInfo.max_value) {
        return false;
    }
    info.id = clapInfo.id;
    info.name = clapInfo.name;
    info.label = clapInfo.module;
    info.minValue = clapInfo.min_value;
    info.maxValue = clapInfo.max_value;
    info.defaultValue = std::clamp(clapInfo.default_value,
                                   clapInfo.min_value, clapInfo.max_value);
    return true;
}

bool PluginInstance::paramGetStringByValue(uint32 paramId, double value,
                                           char* buf, uint32 size) const {
    if (!buf || size == 0) return false;
    buf[0] = '\0';
    ParamInfo info;
    bool found = false;
    for (uint32 index = 0; index < paramsCount(); ++index) {
        if (paramGetInfo(index, info) && info.id == paramId) {
            found = true;
            break;
        }
    }
    if (!found || !std::isfinite(value) || value < info.minValue ||
        value > info.maxValue) return false;
    auto* self = const_cast<PluginInstance*>(this);
    if (!self->resolveParamExt()) return false;
    const auto* extension = static_cast<const clap_plugin_params*>(self->paramExt_);
    const bool ok = extension->value_to_text && extension->value_to_text(
        plugin_, paramId, value, buf, size);
    buf[size - 1] = '\0';
    return ok;
}

bool PluginInstance::paramGetValueByString(uint32 paramId, const char* str,
                                           double& value) {
    if (!str) return false;
    if (!resolveParamExt()) return false;
    const auto* extension = static_cast<const clap_plugin_params*>(paramExt_);
    if (!extension->text_to_value ||
        !extension->text_to_value(plugin_, paramId, str, &value) ||
        !std::isfinite(value)) {
        return false;
    }
    ParamInfo info;
    bool found = false;
    for (uint32 index = 0; index < paramsCount(); ++index) {
        if (paramGetInfo(index, info) && info.id == paramId) {
            found = true;
            value = std::clamp(value, info.minValue, info.maxValue);
            break;
        }
    }
    return found;
}

ClapEffect::ClapEffect(Plugin* plugin, const ArtifactCore::String& name)
    : plugin_(plugin), name_(name) {}

ClapEffect::~ClapEffect() {
    if (plugin_ && active_) {
        plugin_->stopProcessing();
        plugin_->deactivate();
    }
}

uint32 PluginInstance::latencySamples() const
{
    const auto* extension = static_cast<const clap_plugin_latency*>(
        plugin_ && plugin_->get_extension ? plugin_->get_extension(plugin_, "clap.latency") : nullptr);
    return extension && extension->get ? extension->get(plugin_) : 0;
}

uint32 PluginInstance::tailSamples() const
{
    const auto* extension = static_cast<const clap_plugin_tail*>(
        plugin_ && plugin_->get_extension ? plugin_->get_extension(plugin_, "clap.tail") : nullptr);
    return extension && extension->get ? extension->get(plugin_) : 0;
}

qint64 ClapEffect::latencySamples() const
{
    return plugin_ ? static_cast<qint64>(plugin_->latencySamples()) : 0;
}

qint64 ClapEffect::tailSamples() const
{
    return plugin_ ? static_cast<qint64>(plugin_->tailSamples()) : 0;
}

void ClapEffect::process(ArtifactCore::AudioSegment& segment,
                         const ArtifactCore::AudioSegment* /*sideChain*/) {
    if (isBypassed() || !plugin_ || segment.frameCount() <= 0) return;
    const double sampleRate = static_cast<double>(segment.sampleRate);
    if (active_ && activeSampleRate_ != sampleRate) {
        plugin_->stopProcessing();
        plugin_->deactivate();
        active_ = false;
    }
    if (!active_) {
        const auto frames = static_cast<uint32>(segment.frameCount());
        if (!plugin_->activate(sampleRate, frames, frames)) return;
        if (!plugin_->startProcessing()) {
            plugin_->deactivate();
            return;
        }
        active_ = true;
        activeSampleRate_ = sampleRate;
    }
    ArtifactCore::AudioSegment output;
    if (plugin_->processSegment(segment, output)) {
        segment = std::move(output);
    }
}

std::vector<ArtifactCore::EffectParameter> ClapEffect::getParameters() const {
    NamedVector<ArtifactCore::EffectParameter> parameters{
        makeNamedVector<ArtifactCore::EffectParameter>(ContainerName{"ClapEffectParameters"})};
    if (!plugin_) return parameters.toStdVector();
    const uint32 count = plugin_->paramsCount();
    parameters.reserve(count);
    for (uint32 index = 0; index < count; ++index) {
        Plugin::ParamInfo info;
        if (!plugin_->paramGetInfo(index, info)) continue;
        const auto id = ArtifactCore::String(std::string("clap.param.") + std::to_string(info.id));
        parameters.append({id, ArtifactCore::String(info.name),
                              static_cast<float>(info.minValue),
                              static_cast<float>(info.maxValue),
                              static_cast<float>(info.defaultValue),
                              static_cast<float>(plugin_->paramValue(info.id))});
    }
    return parameters.toStdVector();
}

void ClapEffect::setParameterValue(const ArtifactCore::String& id, float value) {
    if (!plugin_) return;
    const std::string key = ArtifactCore::toStdString(id);
    constexpr std::string_view prefix = "clap.param.";
    if (key.rfind(prefix, 0) != 0) return;
    try {
        const uint32 paramId = static_cast<uint32>(std::stoul(key.substr(prefix.size())));
        if (!std::isfinite(value)) return;
        Plugin::ParamInfo info;
        bool found = false;
        for (uint32 index = 0; index < plugin_->paramsCount(); ++index) {
            if (plugin_->paramGetInfo(index, info) && info.id == paramId) {
                found = true;
                break;
            }
        }
        if (!found) return;
        const double clamped = std::clamp(static_cast<double>(value),
                                          info.minValue, info.maxValue);
        plugin_->paramSetValue(paramId, clamped);
    } catch (...) {
    }
}

// ─────────────────────────────────────────────────────────
// Host 実装
// ─────────────────────────────────────────────────────────
class Host::Impl {
public:
    clap_host host{};
    NamedVector<std::string> searchPaths{
        makeNamedVector<std::string>(ContainerName{"ClapHostSearchPaths"})};
    // ライブラリを生きたまま保持（プラグイン生存中のアンロード防止）
    std::vector<std::shared_ptr<PluginLibrary>> libraries;
};

Host::Host() : impl_(new Impl()) {
    impl_->host.host_data = impl_;
    impl_->host.get_extension = hostGetExtension;
    impl_->host.request_restart = hostRequestRestart;
    impl_->host.request_process = hostRequestProcess;
    impl_->host.request_callback = hostRequestCallback;
#ifdef _WIN32
    impl_->searchPaths.assign({
        "C:/Program Files/Common Files/CLAP",
        "C:/Program Files/Common Files/VST3",
    });
#elif __APPLE__
    impl_->searchPaths.assign({
        "/Library/Audio/Plug-Ins/CLAP",
        "~/Library/Audio/Plug-Ins/CLAP",
    });
#else
    impl_->searchPaths.assign({
        "/usr/lib/clap",
        "/usr/local/lib/clap",
        "~/.clap",
    });
#endif
}

Host::~Host() {
    unloadAll();
    delete impl_;
    impl_ = nullptr;
}

void Host::addSearchPath(const std::string& path) {
    impl_->searchPaths.push_back(path);
}

void Host::setSearchPaths(const std::vector<std::string>& paths) {
    impl_->searchPaths.clear();
    impl_->searchPaths.insert(impl_->searchPaths.end(), paths.begin(), paths.end());
}

Plugin* Host::loadPlugin(const std::string& path) {
    if (!fs::exists(path) || !fs::is_regular_file(path)) {
        std::cerr << "[CLAP] File not found: " << path << std::endl;
        return nullptr;
    }

    auto lib = std::make_shared<PluginLibrary>();
    if (!lib->load(path)) {
        std::cerr << "[CLAP] Failed to load: " << path << std::endl;
        return nullptr;
    }

    const auto* entry = lib->entry;
    if (!entry) return nullptr;

    const uint32_t count = entry->get_plugin_count();
    constexpr uint32_t kMaxPluginsPerLibrary = 4096;
    if (count > kMaxPluginsPerLibrary) {
        std::cerr << "[CLAP] Invalid plugin count in: " << path << std::endl;
        return nullptr;
    }
    if (count == 0) {
        std::cerr << "[CLAP] No plugins found in: " << path << std::endl;
        return nullptr;
    }

    Plugin* first = nullptr;

    for (uint32_t i = 0; i < count; ++i) {
        const auto* cd = entry->get_plugin_descriptor(i);
        if (!cd || !cd->id || !*cd->id || !cd->name || !*cd->name ||
            !cd->version || !*cd->version) continue;

        PluginDescriptor pd = makeDescriptor(cd);
        const auto* cp = entry->create_plugin(&impl_->host, i);
        if (!cp) continue;

        auto* instance = new PluginInstance(cp, pd, entry);
        if (!instance->init()) {
            delete instance;
            continue;
        }
        plugins_.push_back(instance);
        if (!first) first = instance;
    }

    impl_->libraries.push_back(lib);
    std::cout << "[CLAP] Loaded " << plugins_.size() << " plugin(s) from: "
              << path << std::endl;
    return first;
}

void Host::unloadPlugin(Plugin* plugin) {
    auto it = std::find(plugins_.begin(), plugins_.end(), plugin);
    if (it != plugins_.end()) {
        (*it)->destroy();
        delete *it;
        plugins_.erase(it);
    }
}

std::unique_ptr<ClapEffect> Host::createEffect(
    Plugin* plugin, const ArtifactCore::String& name) const {
    if (!plugin) return nullptr;
    return std::make_unique<ClapEffect>(plugin, name);
}

void Host::unloadAll() {
    for (auto* p : plugins_) {
        p->destroy();
        delete p;
    }
    plugins_.clear();
    impl_->libraries.clear();
}

std::vector<std::string> Host::scanPlugins() {
    NamedVector<std::string> found{
        makeNamedVector<std::string>(ContainerName{"ClapPluginScanResults"})};
    std::unordered_set<std::string> seen;
    for (const auto& searchPath : impl_->searchPaths) {
        try {
            std::string resolvedPath = searchPath;
            if (!resolvedPath.empty() && resolvedPath.front() == '~' &&
                (resolvedPath.size() == 1 || resolvedPath[1] == '/' ||
                 resolvedPath[1] == '\\')) {
                const char* home = std::getenv("HOME");
#ifdef _WIN32
                if (!home) home = std::getenv("USERPROFILE");
#endif
                if (!home || *home == '\0') continue;
                resolvedPath = std::string(home) + resolvedPath.substr(1);
            }
            if (!fs::exists(resolvedPath) || !fs::is_directory(resolvedPath)) continue;
            for (const auto& entry : fs::recursive_directory_iterator(resolvedPath)) {
                if (!entry.is_regular_file()) continue;
                std::string extension = entry.path().extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (extension != ".clap" && extension != ".dll" && extension != ".so") continue;
                const std::string identity = fs::weakly_canonical(entry.path()).string();
                if (seen.insert(identity).second) found.append(entry.path().string());
            }
        } catch (...) {}
    }
    return found.toStdVector();
}

} // namespace clap
