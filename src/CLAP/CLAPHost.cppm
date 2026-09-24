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

void hostGuiResizeHintsChanged(const clap_host*) {}
bool hostGuiRequestResize(const clap_host*, uint32_t, uint32_t) { return false; }
bool hostGuiRequestShow(const clap_host*) { return false; }
bool hostGuiRequestHide(const clap_host*) { return false; }
void hostGuiClosed(const clap_host*, bool) {}

const clap_host_gui kHostGuiExtension{
    hostGuiResizeHintsChanged, hostGuiRequestResize, hostGuiRequestShow,
    hostGuiRequestHide, hostGuiClosed};

const void* hostGetExtension(const clap_host*, const char* id) {
    return id && std::strcmp(id, "clap.gui") == 0 ? &kHostGuiExtension : nullptr;
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
    const clap_plugin_factory* factory = nullptr;
    bool initialized = false;

    ~PluginLibrary() {
        if (handle) {
            if (entry && initialized) entry->deinit();
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
        if (!entry || entry->clap_version.major < 1 || !entry->init ||
            !entry->deinit || !entry->get_factory) {
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
        initialized = true;
        factory = static_cast<const clap_plugin_factory*>(
            entry->get_factory("clap.plugin-factory"));
        if (!factory || !factory->get_plugin_count ||
            !factory->get_plugin_descriptor || !factory->create_plugin) {
            entry->deinit();
            initialized = false;
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(handle));
#else
            dlclose(handle);
#endif
            handle = nullptr;
            entry = nullptr;
            factory = nullptr;
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
    const clap_event_param_value* events = nullptr;
    uint32_t count = 0;
};

uint32_t pendingInputSize(const clap_input_events* list) {
    const auto* context = static_cast<const PendingInputContext*>(list->ctx);
    return context && context->events ? context->count : 0;
}

const clap_event_header* pendingInputGet(const clap_input_events* list, uint32_t index) {
    const auto* context = static_cast<const PendingInputContext*>(list->ctx);
    if (!context || !context->events || index >= context->count) return nullptr;
    return &context->events[index].header;
}

bool discardOutputEvent(const clap_output_events*, const clap_event_header*) {
    return false;
}

// ─────────────────────────────────────────────────────────
// PluginInstance — const clap_plugin* の具象ラッパー
// ─────────────────────────────────────────────────────────
PluginInstance::PluginInstance(const clap_plugin* plugin,
                               const PluginDescriptor& desc,
                               const clap_plugin_entry* entry)
    : plugin_(plugin), desc_(desc), entry_(entry) {
    for (uint64_t index = 0; index < kPendingParamCapacity; ++index) {
        pendingParams_[index].sequence.store(index, std::memory_order_relaxed);
    }
}

PluginInstance::~PluginInstance() {
    closeEditor();
    if (processing_) stopProcessing();
    if (active_) deactivate();
    if (plugin_ && plugin_->destroy)
        plugin_->destroy(plugin_);
    plugin_ = nullptr;
    paramExt_ = nullptr;
}

bool PluginInstance::init() {
    if (!plugin_ || !plugin_->init || !plugin_->init(plugin_)) return false;
    const auto* audioPorts = static_cast<const clap_plugin_audio_ports*>(
        getExtension("clap.audio-ports"));
    if (!audioPorts || !audioPorts->count || !audioPorts->get ||
        audioPorts->count(plugin_, true) != 1 ||
        audioPorts->count(plugin_, false) != 1) {
        if (plugin_->destroy) plugin_->destroy(plugin_);
        plugin_ = nullptr;
        return false;
    }
    clap_audio_port_info inputInfo{};
    clap_audio_port_info outputInfo{};
    constexpr uint32 kAudioPortIsMain = 1u << 0;
    if (!audioPorts->get(plugin_, 0, true, &inputInfo) ||
        !audioPorts->get(plugin_, 0, false, &outputInfo) ||
        !(inputInfo.flags & kAudioPortIsMain) ||
        !(outputInfo.flags & kAudioPortIsMain) ||
        inputInfo.channel_count == 0 || inputInfo.channel_count > 2 ||
        outputInfo.channel_count == 0 || outputInfo.channel_count > 2) {
        if (plugin_->destroy) plugin_->destroy(plugin_);
        plugin_ = nullptr;
        return false;
    }
    audioInputChannelCount_ = inputInfo.channel_count;
    audioOutputChannelCount_ = outputInfo.channel_count;
    try {
        for (uint32 channel = 0; channel < 2; ++channel) {
            inputScratch_[channel].resize(kProcessMaxFrames);
            outputScratch_[channel].resize(kProcessMaxFrames);
        }
    } catch (...) {
        if (plugin_->destroy) plugin_->destroy(plugin_);
        plugin_ = nullptr;
        return false;
    }
    return true;
}

void PluginInstance::destroy() {
    closeEditor();
    if (processing_) stopProcessing();
    if (active_) deactivate();
    if (plugin_ && plugin_->destroy)
        plugin_->destroy(plugin_);
    plugin_ = nullptr;
    paramExt_ = nullptr;
}

bool PluginInstance::activate(float64 sampleRate, uint32 minFrameCount, uint32 maxFrameCount) {
    if (!plugin_ || !plugin_->activate || active_ || minFrameCount == 0 ||
        maxFrameCount < minFrameCount || maxFrameCount > kProcessMaxFrames) return false;
    active_ = plugin_->activate(plugin_, sampleRate, minFrameCount, maxFrameCount);
    if (active_) maxFrames_ = maxFrameCount;
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
        process.framesCount == 0 || process.framesCount > maxFrames_ ||
        !process.audioInputs || process.audioInputsCount != 1 ||
        !process.audioOutputs || process.audioOutputsCount != 1 ||
        process.audioInputs[0].channelCount != audioInputChannelCount_ ||
        process.audioOutputs[0].channelCount != audioOutputChannelCount_) return false;
    clap_process cp{};
    cp.steady_time = -1;
    cp.frames_count = process.framesCount;
    std::array<float*, 2> inputChannels{};
    std::array<float*, 2> outputChannels{};
    clap_audio_buffer inputBuffer{};
    clap_audio_buffer outputBuffer{};
    // AudioBuffer → deinterleaved float*
    if (process.audioInputs && process.audioInputsCount > 0) {
        const AudioBuffer& input = process.audioInputs[0];
        cp.audio_inputs_count = input.channelCount;
        for (uint32 channel = 0; channel < cp.audio_inputs_count; ++channel) {
            inputChannels[channel] = input.channels[channel];
            if (!inputChannels[channel] && input.channelCount == 1) {
                inputChannels[channel] = input.data32;
            }
            if (!inputChannels[channel]) return false;
        }
        inputBuffer.data32 = inputChannels.data();
        inputBuffer.channel_count = cp.audio_inputs_count;
        cp.audio_inputs = &inputBuffer;
    }
    if (process.audioOutputs && process.audioOutputsCount > 0) {
        const AudioBuffer& output = process.audioOutputs[0];
        cp.audio_outputs_count = output.channelCount;
        for (uint32 channel = 0; channel < cp.audio_outputs_count; ++channel) {
            outputChannels[channel] = output.channels[channel];
            if (!outputChannels[channel] && output.channelCount == 1) {
                outputChannels[channel] = output.data32;
            }
            if (!outputChannels[channel]) return false;
        }
        outputBuffer.data32 = outputChannels.data();
        outputBuffer.channel_count = cp.audio_outputs_count;
        cp.audio_outputs = &outputBuffer;
    }
    uint32_t parameterEventCount = 0;
    PendingParam pending{};
    while (parameterEventCount < processParamEvents_.size() &&
           dequeuePendingParam(pending)) {
        clap_event_param_value& event = processParamEvents_[parameterEventCount];
        event = {};
        event.header.size = sizeof(clap_event_param_value);
        event.header.time = 0;
        event.header.space_id = 0;
        event.header.type = 5; // CLAP_EVENT_PARAM_VALUE
        event.param_id = pending.id;
        event.note_id = -1;
        event.port_index = -1;
        event.channel = -1;
        event.key = -1;
        event.value = pending.value;
        ++parameterEventCount;
    }
    PendingInputContext inputContext{processParamEvents_.data(), parameterEventCount};
    clap_input_events inputEvents{&inputContext, pendingInputSize, pendingInputGet};
    clap_output_events outputEvents{nullptr, discardOutputEvent};
    cp.in_events = &inputEvents;
    cp.out_events = &outputEvents;
    const int32_t status = plugin_->process(plugin_, &cp);
    return status >= 1 && status <= 4;
}

bool PluginInstance::enqueuePendingParam(PendingParam value) {
    uint64_t position = pendingEnqueuePosition_.load(std::memory_order_relaxed);
    for (;;) {
        PendingParamSlot& slot = pendingParams_[position % kPendingParamCapacity];
        const uint64_t sequence = slot.sequence.load(std::memory_order_acquire);
        const int64_t difference = static_cast<int64_t>(sequence - position);
        if (difference == 0) {
            if (pendingEnqueuePosition_.compare_exchange_weak(
                    position, position + 1, std::memory_order_relaxed)) {
                slot.value = value;
                slot.sequence.store(position + 1, std::memory_order_release);
                return true;
            }
        } else if (difference < 0) {
            return false;
        } else {
            position = pendingEnqueuePosition_.load(std::memory_order_relaxed);
        }
    }
}

bool PluginInstance::dequeuePendingParam(PendingParam& value) {
    uint64_t position = pendingDequeuePosition_.load(std::memory_order_relaxed);
    for (;;) {
        PendingParamSlot& slot = pendingParams_[position % kPendingParamCapacity];
        const uint64_t sequence = slot.sequence.load(std::memory_order_acquire);
        const int64_t difference = static_cast<int64_t>(sequence - (position + 1));
        if (difference == 0) {
            if (pendingDequeuePosition_.compare_exchange_weak(
                    position, position + 1, std::memory_order_relaxed)) {
                value = slot.value;
                slot.sequence.store(position + kPendingParamCapacity,
                                    std::memory_order_release);
                return true;
            }
        } else if (difference < 0) {
            return false;
        } else {
            position = pendingDequeuePosition_.load(std::memory_order_relaxed);
        }
    }
}

bool PluginInstance::processSegment(const ArtifactCore::AudioSegment& input,
                                    ArtifactCore::AudioSegment& output,
                                    uint32 frameOffset) {
    const uint32 hostChannels = static_cast<uint32>(input.channelCount());
    const uint32 frames = static_cast<uint32>(input.frameCount());
    if ((hostChannels != 1 && hostChannels != 2) || frames == 0 ||
        frames > maxFrames_ || input.channelData.size() < static_cast<int>(hostChannels) ||
        output.channelCount() != static_cast<int>(hostChannels) ||
        output.frameCount() != static_cast<int>(frames)) return false;
    for (uint32 channel = 0; channel < hostChannels; ++channel) {
        if (input.channelData[static_cast<int>(channel)].size() <
                static_cast<int>(frames) ||
            output.channelData[static_cast<int>(channel)].size() <
                static_cast<int>(frames)) return false;
    }

    AudioBuffer inputBuffer{};
    AudioBuffer outputBuffer{};
    inputBuffer.channelCount = audioInputChannelCount_;
    inputBuffer.frameCount = frames;
    outputBuffer.channelCount = audioOutputChannelCount_;
    outputBuffer.frameCount = frames;
    Process request{};
    request.framesCount = frames;
    request.frameIndex = frameOffset;
    request.audioInputs = &inputBuffer;
    request.audioInputsCount = 1;
    request.audioOutputs = &outputBuffer;
    request.audioOutputsCount = 1;
    if (audioInputChannelCount_ == hostChannels) {
        for (uint32 channel = 0; channel < audioInputChannelCount_; ++channel) {
            const auto& source = input.channelData[static_cast<int>(channel)];
            std::copy_n(source.constData(), frames, inputScratch_[channel].data());
        }
    } else if (audioInputChannelCount_ == 1) {
        const auto& left = input.channelData[0];
        const auto& right = input.channelData[1];
        for (uint32 frame = 0; frame < frames; ++frame) {
            inputScratch_[0][frame] =
                0.5f * left[static_cast<int>(frame)] +
                0.5f * right[static_cast<int>(frame)];
        }
    } else {
        const auto& mono = input.channelData[0];
        std::copy_n(mono.constData(), frames, inputScratch_[0].data());
        std::copy_n(mono.constData(), frames, inputScratch_[1].data());
    }
    for (uint32 channel = 0; channel < audioInputChannelCount_; ++channel) {
        inputBuffer.channels[channel] = inputScratch_[channel].data();
    }
    for (uint32 channel = 0; channel < audioOutputChannelCount_; ++channel) {
        outputBuffer.channels[channel] = outputScratch_[channel].data();
    }
    if (!process(request)) return false;
    if (audioOutputChannelCount_ == hostChannels) {
        for (uint32 channel = 0; channel < hostChannels; ++channel) {
            auto& destination = output.channelData[static_cast<int>(channel)];
            std::copy_n(outputScratch_[channel].data(), frames, destination.data());
        }
    } else if (audioOutputChannelCount_ == 1) {
        const float* mono = outputScratch_[0].data();
        for (uint32 channel = 0; channel < hostChannels; ++channel) {
            auto& destination = output.channelData[static_cast<int>(channel)];
            std::copy_n(mono, frames, destination.data());
        }
    } else {
        auto& destination = output.channelData[0];
        const float* left = outputScratch_[0].data();
        const float* right = outputScratch_[1].data();
        for (uint32 frame = 0; frame < frames; ++frame) {
            destination[static_cast<int>(frame)] =
                0.5f * left[frame] + 0.5f * right[frame];
        }
    }
    return true;
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
    return enqueuePendingParam(
        {paramId, std::clamp(value, info.minValue, info.maxValue)});
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

ClapEffect::ClapEffect(Host* owner, Plugin* plugin,
                       const ArtifactCore::String& name)
    : owner_(owner), plugin_(plugin), name_(name) {}

ClapEffect::~ClapEffect() {
    if (plugin_ && active_) {
        plugin_->stopProcessing();
        plugin_->deactivate();
        active_ = false;
    }
    if (plugin_) {
        plugin_->closeEditor();
        if (owner_) owner_->unloadPlugin(plugin_);
        plugin_ = nullptr;
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

bool PluginInstance::hasEditor() const {
    auto* self = const_cast<PluginInstance*>(this);
    if (!self->plugin_ || !self->plugin_->get_extension) return false;
    if (!self->guiExt_) self->guiExt_ = const_cast<void*>(
        self->plugin_->get_extension(self->plugin_, "clap.gui"));
    const auto* gui = static_cast<const clap_plugin_gui*>(self->guiExt_);
    return gui && gui->is_api_supported && gui->create && gui->destroy &&
           gui->get_size && gui->set_parent && gui->show;
}

bool PluginInstance::openEditor(
    const clap_window& parent, void* resizeContext,
    bool (*resizeCallback)(void*, int32, int32), uint32& width,
    uint32& height, bool& resizable) {
    width = 0;
    height = 0;
    resizable = false;
    if (guiCreated_ || !parent.api || !hasEditor()) return false;
    auto* gui = static_cast<const clap_plugin_gui*>(guiExt_);
    if (!gui->is_api_supported(plugin_, parent.api, false) ||
        !gui->create(plugin_, parent.api, false)) return false;
    guiCreated_ = true;
    editorParent_ = parent;
    editorApi_ = parent.api;
    uint32_t requestedWidth = 0;
    uint32_t requestedHeight = 0;
    if (!gui->get_size(plugin_, &requestedWidth, &requestedHeight) ||
        requestedWidth == 0 || requestedHeight == 0 ||
        requestedWidth > 8192 || requestedHeight > 8192 ||
        (resizeCallback && !resizeCallback(
            resizeContext, static_cast<int32>(requestedWidth),
            static_cast<int32>(requestedHeight))) ||
        !gui->set_parent(plugin_, &editorParent_)) {
        closeEditor();
        return false;
    }
    guiResizable_ = gui->can_resize && gui->can_resize(plugin_);
    if (!gui->show(plugin_)) {
        closeEditor();
        return false;
    }
    guiVisible_ = true;
    width = requestedWidth;
    height = requestedHeight;
    resizable = guiResizable_;
    return true;
}

bool PluginInstance::resizeEditor(uint32& width, uint32& height) {
    if (!guiCreated_ || !guiResizable_ || width == 0 || height == 0 ||
        width > 8192 || height > 8192) return false;
    const auto* gui = static_cast<const clap_plugin_gui*>(guiExt_);
    if (!gui || !gui->adjust_size || !gui->set_size) return false;
    uint32 adjustedWidth = width;
    uint32 adjustedHeight = height;
    if (!gui->adjust_size(plugin_, &adjustedWidth, &adjustedHeight) ||
        adjustedWidth == 0 || adjustedHeight == 0 ||
        adjustedWidth > 8192 || adjustedHeight > 8192) return false;
    if (!gui->set_size(plugin_, adjustedWidth, adjustedHeight)) return false;
    width = adjustedWidth;
    height = adjustedHeight;
    return true;
}

void PluginInstance::closeEditor() {
    if (!guiCreated_) return;
    const auto* gui = static_cast<const clap_plugin_gui*>(guiExt_);
    if (gui) {
        if (guiVisible_ && gui->hide) (void)gui->hide(plugin_);
        if (gui->destroy) gui->destroy(plugin_);
    }
    guiCreated_ = false;
    guiVisible_ = false;
    guiResizable_ = false;
    guiExt_ = nullptr;
    editorApi_.clear();
    editorParent_ = {};
}

bool ClapEffect::hasEditor() const {
    return plugin_ && plugin_->hasEditor();
}

bool ClapEffect::openEditor(
    const clap_window& parent, void* resizeContext,
    bool (*resizeCallback)(void*, int32, int32), uint32& width,
    uint32& height, bool& resizable) {
    return plugin_ && plugin_->openEditor(
        parent, resizeContext, resizeCallback, width, height, resizable);
}

bool ClapEffect::resizeEditor(uint32& width, uint32& height) {
    return plugin_ && plugin_->resizeEditor(width, height);
}

void ClapEffect::closeEditor() {
    if (plugin_) plugin_->closeEditor();
}

void ClapEffect::process(ArtifactCore::AudioSegment& segment,
                         const ArtifactCore::AudioSegment* /*sideChain*/) {
    if (isBypassed() || !plugin_ || segment.frameCount() <= 0) return;
    const double sampleRate = static_cast<double>(segment.sampleRate);
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0) return;
    if (active_ && activeSampleRate_ != sampleRate) {
        plugin_->stopProcessing();
        plugin_->deactivate();
        active_ = false;
    }
    if (!active_) {
        if (!plugin_->activate(sampleRate, 1, kProcessMaxFrames)) return;
        if (!plugin_->startProcessing()) {
            plugin_->deactivate();
            return;
        }
        active_ = true;
        activeSampleRate_ = sampleRate;
    }
    plugin_->processSegment(segment, segment);
}

std::vector<ArtifactCore::EffectParameter> ClapEffect::getParameters() const {
    ArtifactCore::NamedVector<ArtifactCore::EffectParameter> parameters{
        ArtifactCore::makeNamedVector<ArtifactCore::EffectParameter>(ArtifactCore::ContainerName{"ClapEffectParameters"})};
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
    ArtifactCore::NamedVector<std::string> searchPaths{
        ArtifactCore::makeNamedVector<std::string>(ArtifactCore::ContainerName{"ClapHostSearchPaths"})};
    // ライブラリを生きたまま保持（プラグイン生存中のアンロード防止）
    std::vector<std::shared_ptr<PluginLibrary>> libraries;
};

Host::Host() : impl_(new Impl()) {
    impl_->host.clap_version = {1, 2, 10};
    impl_->host.host_data = impl_;
    impl_->host.name = "ArtifactStudio";
    impl_->host.vendor = "ArtifactStudio";
    impl_->host.url = "https://github.com/lagmajin/ArtifactStudio";
    impl_->host.version = "0.1.0";
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
    for (const auto& path : paths) {
        impl_->searchPaths.push_back(path);
    }
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
    const auto* factory = lib->factory;
    if (!factory) return nullptr;

    const uint32_t count = factory->get_plugin_count(factory);
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
        const auto* cd = factory->get_plugin_descriptor(factory, i);
        if (!cd || cd->clap_version.major < 1 || !cd->id || !*cd->id ||
            !cd->name || !*cd->name || !cd->version || !*cd->version) continue;

        PluginDescriptor pd = makeDescriptor(cd);
        const auto* cp = factory->create_plugin(factory, &impl_->host, cd->id);
        if (!cp) continue;

        auto* instance = new PluginInstance(cp, pd, entry);
        if (!instance->init()) {
            delete instance;
            continue;
        }
        plugins_.push_back(instance);
        if (!first) first = instance;
        break;
    }

    if (!first) return nullptr;
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
    Plugin* plugin, const ArtifactCore::String& name) {
    if (!plugin) return nullptr;
    return std::make_unique<ClapEffect>(this, plugin, name);
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
    ArtifactCore::NamedVector<std::string> found{
        ArtifactCore::makeNamedVector<std::string>(ArtifactCore::ContainerName{"ClapPluginScanResults"})};
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
