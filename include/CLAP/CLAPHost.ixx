module;
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <array>

// ─────────────────────────────────────────────────────────
// Minimal CLAP C API structs — clap.h 非依存
// ─────────────────────────────────────────────────────────
struct clap_plugin_descriptor {
    const char* clap_version;
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

struct clap_host {
    void* host_data;
    // 必要に応じて拡張 (timer, request_restart 等)
};

struct clap_process {
    void* reserved[2];           // in/out events
    uint32_t frames_count;
    uint32_t frame_offset;
    float* audio_inputs[2];       // deinterleaved: [0]=左 [1]=右
    float* audio_outputs[2];
    uint32_t audio_inputs_count;
    uint32_t audio_outputs_count;
    double transport;
};

struct clap_plugin {
    const clap_plugin_descriptor* desc;
    void* plugin_data;
    bool (*init)(const struct clap_plugin*);
    void (*destroy)(const struct clap_plugin*);
    bool (*activate)(const struct clap_plugin*, double sample_rate,
                     uint32_t min_frames, uint32_t max_frames);
    void (*deactivate)(const struct clap_plugin*);
    bool (*start_processing)(const struct clap_plugin*);
    void (*stop_processing)(const struct clap_plugin*);
    bool (*process)(const struct clap_plugin*, const struct clap_process*);
    const void* (*get_extension)(const struct clap_plugin*, const char* id);
    void (*on_main_thread)(const struct clap_plugin*);
};

struct clap_plugin_entry {
    uint32_t clap_version;
    bool (*init)(const char* plugin_path);
    void (*deinit)();
    uint32_t (*get_plugin_count)();
    const clap_plugin_descriptor* (*get_plugin_descriptor)(uint32_t index);
    const clap_plugin* (*create_plugin)(const struct clap_host* host, uint32_t index);
    void (*destroy_plugin)(const struct clap_plugin* plugin);
};

export module CLAP.Host;

import Audio.Segment;

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
// CLAP DLL は const clap_plugin_entry* clap_entry をエクスポートする
// 関数ポインタとして解決する: const clap_plugin_entry* (*)()

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
    bool processSegment(const ArtifactCore::AudioSegment& input,
                        ArtifactCore::AudioSegment& output,
                        uint32 frameOffset = 0) {
        uint32 ch = static_cast<uint32>(input.channelCount());
        uint32 frames = static_cast<uint32>(input.frameCount());
        if (ch == 0 || frames == 0 || frames > kProcessMaxFrames) return false;
        output.channelData.resize(static_cast<int>(ch));
        for (uint32 i = 0; i < ch; ++i)
            output.channelData[static_cast<int>(i)].resize(static_cast<int>(frames));
        AudioBuffer inBuf, outBuf;
        inBuf.channelCount = ch;
        inBuf.frameCount = frames;
        outBuf.channelCount = ch;
        outBuf.frameCount = frames;
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
    const void* getExtension(const char* id) override;
    const PluginDescriptor& descriptor() const override { return desc_; }

    // パラメータ
    uint32 paramsCount() const override;
    bool paramInfo(uint32 index, void* info) const override;
    double paramValue(uint32 paramId) const override;
    bool paramSetValue(uint32 paramId, double value) override;
    bool paramGetDisplay(uint32 paramId, char* buf, uint32 size) const override;

private:
    const struct clap_plugin* plugin_;
    PluginDescriptor desc_;
    const struct clap_plugin_entry* entry_;
    // clap_plugin_params 拡張 (遅延解決)
    void* paramExt_ = nullptr;
    bool resolveParamExt();
};

class Host {
public:
    Host();
    ~Host();

    // プラグイン読み込み
    Plugin* loadPlugin(const std::string& path);
    void unloadPlugin(Plugin* plugin);
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
