module;
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#ifdef _WIN32
#define CLAP_HOST_ABI __cdecl
#else
#define CLAP_HOST_ABI
#endif

export module CLAP.Host;

import Audio.Segment;
import Audio.Effect;

// ─────────────────────────────────────────────────────────
// Minimal CLAP C API structs — clap.h 非依存
// ─────────────────────────────────────────────────────────
export struct clap_version_t {
    uint32_t major;
    uint32_t minor;
    uint32_t revision;
};

export struct clap_plugin_descriptor {
    clap_version_t clap_version;
    const char* id;
    const char* name;
    const char* vendor;
    const char* url;
    const char* manual_url;
    const char* support_url;
    const char* version;
    const char* description;
    const char** features;
};

export struct clap_host {
    clap_version_t clap_version;
    void* host_data;
    const char* name;
    const char* vendor;
    const char* url;
    const char* version;
    const void* (CLAP_HOST_ABI *get_extension)(const struct clap_host*, const char*);
    void (CLAP_HOST_ABI *request_restart)(const struct clap_host*);
    void (CLAP_HOST_ABI *request_process)(const struct clap_host*);
    void (CLAP_HOST_ABI *request_callback)(const struct clap_host*);
};

export struct clap_process {
    int64_t steady_time;
    uint32_t frames_count;
    const void* transport;
    const struct clap_audio_buffer* audio_inputs;
    struct clap_audio_buffer* audio_outputs;
    uint32_t audio_inputs_count;
    uint32_t audio_outputs_count;
    const struct clap_input_events* in_events;
    const struct clap_output_events* out_events;
};

export struct clap_audio_buffer {
    float** data32;
    double** data64;
    uint32_t channel_count;
    uint32_t latency;
    uint64_t constant_mask;
};

export struct clap_event_header {
    uint32_t size;
    uint32_t time;
    uint16_t space_id;
    uint16_t type;
    uint32_t flags;
};

export struct clap_event_param_value {
    clap_event_header header;
    uint32_t param_id;
    void* cookie;
    int32_t note_id;
    int16_t port_index;
    int16_t channel;
    int16_t key;
    double value;
};

export struct clap_input_events {
    void* ctx;
    uint32_t (CLAP_HOST_ABI *size)(const clap_input_events* list);
    const clap_event_header* (CLAP_HOST_ABI *get)(const clap_input_events* list, uint32_t index);
};

export struct clap_output_events {
    void* ctx;
    bool (CLAP_HOST_ABI *try_push)(const clap_output_events* list,
                     const clap_event_header* event);
};

export struct clap_plugin {
    const clap_plugin_descriptor* desc;
    void* plugin_data;
    bool (CLAP_HOST_ABI *init)(const struct clap_plugin*);
    void (CLAP_HOST_ABI *destroy)(const struct clap_plugin*);
    bool (CLAP_HOST_ABI *activate)(const struct clap_plugin*, double sample_rate,
                     uint32_t min_frames, uint32_t max_frames);
    void (CLAP_HOST_ABI *deactivate)(const struct clap_plugin*);
    bool (CLAP_HOST_ABI *start_processing)(const struct clap_plugin*);
    void (CLAP_HOST_ABI *stop_processing)(const struct clap_plugin*);
    void (CLAP_HOST_ABI *reset)(const struct clap_plugin*);
    int32_t (CLAP_HOST_ABI *process)(const struct clap_plugin*, const struct clap_process*);
    const void* (CLAP_HOST_ABI *get_extension)(const struct clap_plugin*, const char* id);
    void (CLAP_HOST_ABI *on_main_thread)(const struct clap_plugin*);
};

export struct clap_plugin_latency {
    uint32_t (CLAP_HOST_ABI *get)(const clap_plugin* plugin);
};

export struct clap_plugin_tail {
    uint32_t (CLAP_HOST_ABI *get)(const clap_plugin* plugin);
};

export struct clap_audio_port_info {
    uint32_t id;
    char name[256];
    uint32_t flags;
    uint32_t channel_count;
    const char* port_type;
    uint32_t in_place_pair;
};

export struct clap_plugin_audio_ports {
    uint32_t (CLAP_HOST_ABI *count)(const clap_plugin*, bool is_input);
    bool (CLAP_HOST_ABI *get)(const clap_plugin*, uint32_t index,
                              bool is_input, clap_audio_port_info* info);
};

export struct clap_param_info {
    uint32_t id;
    uint32_t flags;
    void* cookie;
    char name[256];
    char module[1024];
    double min_value;
    double max_value;
    double default_value;
};

export struct clap_plugin_params {
    uint32_t (CLAP_HOST_ABI *count)(const clap_plugin* plugin);
    bool (CLAP_HOST_ABI *get_info)(const clap_plugin* plugin, uint32_t param_index,
                     clap_param_info* param_info);
    bool (CLAP_HOST_ABI *get_value)(const clap_plugin* plugin, uint32_t param_id,
                      double* value);
    bool (CLAP_HOST_ABI *value_to_text)(const clap_plugin* plugin, uint32_t param_id,
                          double value, char* out_buffer, uint32_t out_buffer_capacity);
    bool (CLAP_HOST_ABI *text_to_value)(const clap_plugin* plugin, uint32_t param_id,
                          const char* param_value_text, double* value);
    void (CLAP_HOST_ABI *flush)(const clap_plugin* plugin,
                  const clap_input_events* in_events,
                  const clap_output_events* out_events);
};

export struct clap_window {
    const char* api;
    union {
        void* cocoa;
        void* uikit;
        unsigned long x11;
        void* win32;
        void* ptr;
    };
};

export struct clap_gui_resize_hints {
    bool can_resize_horizontally;
    bool can_resize_vertically;
    bool preserve_aspect_ratio;
    uint32_t aspect_ratio_width;
    uint32_t aspect_ratio_height;
};

export struct clap_plugin_gui {
    bool (CLAP_HOST_ABI *is_api_supported)(const clap_plugin*, const char*, bool);
    bool (CLAP_HOST_ABI *get_preferred_api)(const clap_plugin*, const char**, bool*);
    bool (CLAP_HOST_ABI *create)(const clap_plugin*, const char*, bool);
    void (CLAP_HOST_ABI *destroy)(const clap_plugin*);
    bool (CLAP_HOST_ABI *set_scale)(const clap_plugin*, double);
    bool (CLAP_HOST_ABI *get_size)(const clap_plugin*, uint32_t*, uint32_t*);
    bool (CLAP_HOST_ABI *can_resize)(const clap_plugin*);
    bool (CLAP_HOST_ABI *get_resize_hints)(const clap_plugin*, clap_gui_resize_hints*);
    bool (CLAP_HOST_ABI *adjust_size)(const clap_plugin*, uint32_t*, uint32_t*);
    bool (CLAP_HOST_ABI *set_size)(const clap_plugin*, uint32_t, uint32_t);
    bool (CLAP_HOST_ABI *set_parent)(const clap_plugin*, const clap_window*);
    bool (CLAP_HOST_ABI *set_transient)(const clap_plugin*, const clap_window*);
    void (CLAP_HOST_ABI *suggest_title)(const clap_plugin*, const char*);
    bool (CLAP_HOST_ABI *show)(const clap_plugin*);
    bool (CLAP_HOST_ABI *hide)(const clap_plugin*);
};

export struct clap_host_gui {
    void (CLAP_HOST_ABI *resize_hints_changed)(const clap_host*);
    bool (CLAP_HOST_ABI *request_resize)(const clap_host*, uint32_t, uint32_t);
    bool (CLAP_HOST_ABI *request_show)(const clap_host*);
    bool (CLAP_HOST_ABI *request_hide)(const clap_host*);
    void (CLAP_HOST_ABI *closed)(const clap_host*, bool);
};

export struct clap_plugin_entry {
    clap_version_t clap_version;
    bool (CLAP_HOST_ABI *init)(const char* plugin_path);
    void (CLAP_HOST_ABI *deinit)();
    const void* (CLAP_HOST_ABI *get_factory)(const char* factory_id);
};

export struct clap_plugin_factory {
    uint32_t (CLAP_HOST_ABI *get_plugin_count)(const clap_plugin_factory* factory);
    const clap_plugin_descriptor* (CLAP_HOST_ABI *get_plugin_descriptor)(
        const clap_plugin_factory* factory, uint32_t index);
    const clap_plugin* (CLAP_HOST_ABI *create_plugin)(const clap_plugin_factory* factory,
                                        const clap_host* host,
                                        const char* plugin_id);
};

#undef CLAP_HOST_ABI

/// CLAP (CLever Audio Plugin) ホスト実装
/// MIT License - clap.h 非依存のホスト側定義

export namespace clap {

// === 基本型 ===
using uint32 = uint32_t;
using int32  = int32_t;
using int64  = int64_t;
using float64 = double;
constexpr int32 kProcessMaxFrames = 8192;

// === プラグイン情報 ===
struct PluginDescriptor {
    std::string id;           // "com.u-he.diva"
    std::string name;         // "Diva"
    std::string vendor;       // "u-he"
    std::string url;
    std::string manualUrl;
    std::string supportUrl;
    std::string version;      // "1.4.3"
    std::string description;
    std::vector<std::string> features; // "instrument", "effect", "synth"...
};

// === オーディオバッファ ===
struct AudioBuffer {
    float* data32 = nullptr;  // インターリーブ or 個別チャンネル
    std::array<float*, 2> channels{}; // deinterleaved channel pointers
    uint32 channelCount = 0;
    uint32 frameCount = 0;
    bool isConstantMask = false;
};

// === イベント ===
enum class EventType : uint16_t {
    NoteOn = 0,
    NoteOff = 1,
    NoteChoke = 2,
    NoteExpression = 3,
    ParamValue = 4,
    ParamMod = 5,
    ParamGestureBegin = 6,
    ParamGestureEnd = 7,
    Transport = 8,
    Midi = 9,
    MidiSysEx = 10,
};

struct EventHeader {
    EventType type;
    uint32 time;  // sample offset in block
    uint32 flags;
};

struct NoteEvent {
    EventHeader header;
    int16_t noteId;
    int16_t portIndex;
    int16_t channel;
    int16_t key;
    float64 velocity;
    float64 pitch;   // -1200..+1200 cent detune
};

struct ParamEvent {
    EventHeader header;
    int32 paramId;
    float64 value;
    float64 modulation;
};

struct TransportEvent {
    EventHeader header;
    int64 songPosFrames;
    // flags for play state, tempo, loop...
};

// === プロセス ===
enum class ProcessFlags : uint32_t {
    kNone = 0,
    kTail = 1 << 0,
    kReplaces = 1 << 1,
};

struct Process {
    ProcessFlags flags = ProcessFlags::kNone;
    uint32 framesCount = 0;
    uint32 frameIndex = 0;  // for timing
    AudioBuffer* audioInputs = nullptr;
    uint32 audioInputsCount = 0;
    AudioBuffer* audioOutputs = nullptr;
    uint32 audioOutputsCount = 0;
    // events...
};

// === プラグインエントリポイント ===
// CLAP module は clap_plugin_entry オブジェクトを `clap_entry` として export する。

// === プラグインインスタンス ===
class Plugin {
public:
    virtual ~Plugin() = default;

    virtual bool init() = 0;
    virtual void destroy() = 0;
    virtual bool activate(float64 sampleRate, uint32 minFrameCount,
                          uint32 maxFrameCount) = 0;
    virtual void deactivate() = 0;
    virtual bool startProcessing() = 0;
    virtual void stopProcessing() = 0;
    virtual bool process(const Process& process) = 0;

    // AudioSegment との変換付き処理
    virtual bool processSegment(const ArtifactCore::AudioSegment& input,
                                ArtifactCore::AudioSegment& output,
                                uint32 frameOffset = 0) {
        uint32 ch = static_cast<uint32>(input.channelCount());
        uint32 frames = static_cast<uint32>(input.frameCount());
        if (ch == 0 || ch > 2 || frames == 0 || frames > kProcessMaxFrames ||
            input.channelData.size() < static_cast<int>(ch) ||
            !std::isfinite(static_cast<double>(input.sampleRate)) ||
            input.sampleRate <= 0.0f) return false;
        for (uint32 channel = 0; channel < ch; ++channel) {
            if (input.channelData[static_cast<int>(channel)].size() <
                static_cast<int>(frames)) return false;
        }
        output.channelData.resize(static_cast<int>(ch));
        output.sampleRate = input.sampleRate;
        output.layout = input.layout;
        output.startFrame = input.startFrame + static_cast<qint64>(frameOffset);
        for (uint32 i = 0; i < ch; ++i)
            output.channelData[static_cast<int>(i)].resize(static_cast<int>(frames));
        AudioBuffer inBuf, outBuf;
        inBuf.channelCount = ch;
        inBuf.frameCount = frames;
        outBuf.channelCount = ch;
        outBuf.frameCount = frames;
        for (uint32 channel = 0; channel < std::min<uint32>(ch, 2); ++channel) {
            inBuf.channels[channel] = const_cast<float*>(input.channelData[static_cast<int>(channel)].constData());
            outBuf.channels[channel] = output.channelData[static_cast<int>(channel)].data();
        }
        inBuf.data32 = ch > 0 ? inBuf.channels[0] : nullptr;
        outBuf.data32 = ch > 0 ? outBuf.channels[0] : nullptr;
        Process proc;
        proc.framesCount = frames;
        proc.frameIndex = frameOffset;
        proc.audioInputs = &inBuf;
        proc.audioInputsCount = 1;
        proc.audioOutputs = &outBuf;
        proc.audioOutputsCount = 1;
        return process(proc);
    }

    virtual const void* getExtension(const char* id) = 0;
    virtual const PluginDescriptor& descriptor() const = 0;

    virtual uint32 latencySamples() const { return 0; }
    virtual uint32 tailSamples() const { return 0; }
    virtual bool hasEditor() const { return false; }
    virtual bool openEditor(const clap_window& parent, void* resizeContext,
                            bool (*resizeCallback)(void*, int32, int32),
                            uint32& width, uint32& height, bool& resizable) {
        (void)parent; (void)resizeContext; (void)resizeCallback;
        (void)width; (void)height; (void)resizable; return false;
    }
    virtual bool resizeEditor(uint32& width, uint32& height) {
        (void)width; (void)height; return false;
    }
    virtual void closeEditor() {}

    // パラメータ
    virtual uint32 paramsCount() const = 0;
    virtual bool paramInfo(uint32 index, void* info) const = 0;
    virtual double paramValue(uint32 paramId) const = 0;
    virtual bool paramSetValue(uint32 paramId, double value) = 0;
    virtual bool paramGetDisplay(uint32 paramId, char* buf, uint32 size) const = 0;

    // パラメータ管理の便宜メソッド
    struct ParamInfo {
        uint32 id = 0;
        std::string name;
        std::string label;
        double minValue = 0.0;
        double maxValue = 1.0;
        double defaultValue = 0.5;
    };
    virtual bool paramGetInfo(uint32 index, ParamInfo& info) const {
        (void)index; (void)info; return false;
    }
    virtual bool paramGetStringByValue(uint32 paramId, double value, char* buf, uint32 size) const {
        (void)paramId; (void)value; (void)buf; (void)size; return false;
    }
    virtual bool paramGetValueByString(uint32 paramId, const char* str, double& value) {
        (void)paramId; (void)str; (void)value; return false;
    }
};

class Host;

class ClapEffect final : public ArtifactCore::AudioEffect {
public:
    explicit ClapEffect(Host* owner, Plugin* plugin,
                        const ArtifactCore::String& name = ArtifactCore::String("CLAP Effect"));
    ~ClapEffect() override;

    ArtifactCore::String getName() const override { return name_; }
    ArtifactCore::String effectType() const override { return ArtifactCore::String("clap"); }
    void process(ArtifactCore::AudioSegment& segment,
                 const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    qint64 latencySamples() const override;
    qint64 tailSamples() const override;
    std::vector<ArtifactCore::EffectParameter> getParameters() const override;
    void setParameterValue(const ArtifactCore::String& id, float value) override;
    bool hasEditor() const;
    bool openEditor(const clap_window& parent, void* resizeContext,
                    bool (*resizeCallback)(void*, int32, int32),
                    uint32& width, uint32& height, bool& resizable);
    bool resizeEditor(uint32& width, uint32& height);
    void closeEditor();

private:
    Host* owner_ = nullptr;
    Plugin* plugin_ = nullptr;
    ArtifactCore::String name_;
    bool active_ = false;
    double activeSampleRate_ = 0.0;
};

// === ホスト（プラグインローダー兼マネージャ）===

// 具象プラグインインスタンス — ロード済み clap_plugin をラップ
class PluginInstance : public Plugin {
public:
    PluginInstance(const struct clap_plugin* plugin,
                   const PluginDescriptor& desc,
                   const struct clap_plugin_entry* entry);
    ~PluginInstance() override;

    bool init() override;
    void destroy() override;
    bool activate(float64 sampleRate, uint32 minFrameCount,
                  uint32 maxFrameCount) override;
    void deactivate() override;
    bool startProcessing() override;
    void stopProcessing() override;
    bool process(const Process& process) override;
    bool processSegment(const ArtifactCore::AudioSegment& input,
                        ArtifactCore::AudioSegment& output,
                        uint32 frameOffset = 0) override;
    const void* getExtension(const char* id) override;
    const PluginDescriptor& descriptor() const override { return desc_; }
    uint32 latencySamples() const override;
    uint32 tailSamples() const override;
    bool hasEditor() const override;
    bool openEditor(const clap_window& parent, void* resizeContext,
                    bool (*resizeCallback)(void*, int32, int32),
                    uint32& width, uint32& height, bool& resizable) override;
    bool resizeEditor(uint32& width, uint32& height) override;
    void closeEditor() override;

    // パラメータ
    uint32 paramsCount() const override;
    bool paramInfo(uint32 index, void* info) const override;
    double paramValue(uint32 paramId) const override;
    bool paramSetValue(uint32 paramId, double value) override;
    bool paramGetDisplay(uint32 paramId, char* buf, uint32 size) const override;
    bool paramGetInfo(uint32 index, ParamInfo& info) const override;
    bool paramGetStringByValue(uint32 paramId, double value, char* buf, uint32 size) const override;
    bool paramGetValueByString(uint32 paramId, const char* str, double& value) override;

private:
    struct PendingParam {
        uint32 id = 0;
        double value = 0.0;
    };
    struct PendingParamSlot {
        std::atomic<uint64_t> sequence{0};
        PendingParam value{};
    };
    static constexpr uint64_t kPendingParamCapacity = 256;
    bool enqueuePendingParam(PendingParam value);
    bool dequeuePendingParam(PendingParam& value);
    const struct clap_plugin* plugin_;
    PluginDescriptor desc_;
    const struct clap_plugin_entry* entry_;
    bool active_ = false;
    bool processing_ = false;
    // clap_plugin_params 拡張 (遅延解決)
    void* paramExt_ = nullptr;
    bool resolveParamExt();
    std::array<PendingParamSlot, kPendingParamCapacity> pendingParams_{};
    std::atomic<uint64_t> pendingEnqueuePosition_{0};
    std::atomic<uint64_t> pendingDequeuePosition_{0};
    std::array<clap_event_param_value, kPendingParamCapacity> processParamEvents_{};
    std::array<std::vector<float>, 2> inputScratch_;
    std::array<std::vector<float>, 2> outputScratch_;
    uint32 maxFrames_ = 0;
    uint32 audioInputChannelCount_ = 0;
    uint32 audioOutputChannelCount_ = 0;
    void* guiExt_ = nullptr;
    bool guiCreated_ = false;
    bool guiResizable_ = false;
    bool guiVisible_ = false;
    clap_window editorParent_{};
    std::string editorApi_;
};

class Host {
public:
    Host();
    ~Host();

    // プラグイン読み込み
    Plugin* loadPlugin(const std::string& path);
    void unloadPlugin(Plugin* plugin);
    std::unique_ptr<ClapEffect> createEffect(
        Plugin* plugin,
        const ArtifactCore::String& name = ArtifactCore::String("CLAP Effect"));
    void unloadAll();

    // 検索パス管理
    void addSearchPath(const std::string& path);
    void setSearchPaths(const std::vector<std::string>& paths);
    std::vector<std::string> scanPlugins();

    // プラグイン一覧
    size_t pluginCount() const { return plugins_.size(); }
    Plugin* pluginAt(size_t index) const { return plugins_[index]; }

private:
    class Impl;
    Impl* impl_;
    std::vector<Plugin*> plugins_;
};

} // namespace clap
