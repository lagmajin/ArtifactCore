module;
#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif
#include <cstring>
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

module CLAP.Host;

namespace clap {

namespace fs = std::filesystem;

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
        // CLAP entry は DLL から const clap_plugin_entry* へのポインタとしてエクスポートされる
        entry = *static_cast<const clap_plugin_entry**>(sym);
        if (!entry || !entry->init) {
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
    if (cd->manual_url) pd.manual_url = cd->manual_url;
    if (cd->support_url) pd.support_url = cd->support_url;
    if (cd->version) pd.version = cd->version;
    if (cd->description) pd.description = cd->description;
    if (cd->features) {
        for (const char** f = cd->features; *f; ++f)
            pd.features.emplace_back(*f);
    }
    return pd;
}

// ─────────────────────────────────────────────────────────
// PluginInstance — const clap_plugin* の具象ラッパー
// ─────────────────────────────────────────────────────────
PluginInstance::PluginInstance(const clap_plugin* plugin,
                               const PluginDescriptor& desc,
                               const clap_plugin_entry* entry)
    : plugin_(plugin), desc_(desc), entry_(entry) {}

PluginInstance::~PluginInstance() {
    if (plugin_ && plugin_->destroy)
        plugin_->destroy(plugin_);
}

bool PluginInstance::init() {
    return plugin_ && plugin_->init && plugin_->init(plugin_);
}

void PluginInstance::destroy() {
    if (plugin_ && plugin_->destroy)
        plugin_->destroy(plugin_);
    plugin_ = nullptr;
}

bool PluginInstance::activate(float64 sampleRate, uint32 minFrameCount, uint32 maxFrameCount) {
    return plugin_ && plugin_->activate &&
        plugin_->activate(plugin_, sampleRate, minFrameCount, maxFrameCount);
}

void PluginInstance::deactivate() {
    if (plugin_ && plugin_->deactivate)
        plugin_->deactivate(plugin_);
}

bool PluginInstance::startProcessing() {
    return plugin_ && plugin_->start_processing &&
        plugin_->start_processing(plugin_);
}

void PluginInstance::stopProcessing() {
    if (plugin_ && plugin_->stop_processing)
        plugin_->stop_processing(plugin_);
}

bool PluginInstance::process(const Process& process) {
    if (!plugin_ || !plugin_->process) return false;
    clap_process cp{};
    cp.frames_count = process.framesCount;
    cp.frame_offset = process.frameIndex;
    // AudioBuffer → deinterleaved float*
    if (process.audioInputs && process.audioInputsCount > 0) {
        cp.audio_inputs_count = process.audioInputs[0].channelCount;
        cp.audio_outputs_count = process.audioOutputs[0].channelCount;
        // Use first buffer — assumes deinterleaved channelData layout
        // In practice, CLAP expects deinterleaved, so we point at channelData vectors
        // Set audio_inputs / audio_outputs to null for now; real multichannel needs
        // proper deinterleaved buffer setup matching AudioSegment
    }
    return plugin_->process(plugin_, &cp);
}

const void* PluginInstance::getExtension(const char* id) {
    return plugin_ && plugin_->get_extension
        ? plugin_->get_extension(plugin_, id) : nullptr;
}

bool PluginInstance::resolveParamExt() {
    if (paramExt_) return true;
    paramExt_ = const_cast<void*>(getExtension("clap.plugin-params"));
    return paramExt_ != nullptr;
}

uint32 PluginInstance::paramsCount() const {
    // Without the full clap_param_info struct, return 0
    return 0;
}

bool PluginInstance::paramInfo(uint32 index, void* info) const {
    (void)index; (void)info; return false;
}

double PluginInstance::paramValue(uint32 paramId) const {
    (void)paramId; return 0.0;
}

bool PluginInstance::paramSetValue(uint32 paramId, double value) {
    (void)paramId; (void)value; return false;
}

bool PluginInstance::paramGetDisplay(uint32 paramId, char* buf, uint32 size) const {
    (void)paramId; (void)buf; (void)size; return false;
}

// ─────────────────────────────────────────────────────────
// Host 実装
// ─────────────────────────────────────────────────────────
class Host::Impl {
public:
    std::vector<std::string> searchPaths;
    // ライブラリを生きたまま保持（プラグイン生存中のアンロード防止）
    std::vector<std::shared_ptr<PluginLibrary>> libraries;
};

Host::Host() : impl_(new Impl()) {
#ifdef _WIN32
    impl_->searchPaths = {
        "C:/Program Files/Common Files/CLAP",
        "C:/Program Files/Common Files/VST3",
    };
#elif __APPLE__
    impl_->searchPaths = {
        "/Library/Audio/Plug-Ins/CLAP",
        "~/Library/Audio/Plug-Ins/CLAP",
    };
#else
    impl_->searchPaths = {
        "/usr/lib/clap",
        "/usr/local/lib/clap",
        "~/.clap",
    };
#endif
}

Host::~Host() { unloadAll(); }

void Host::addSearchPath(const std::string& path) {
    impl_->searchPaths.push_back(path);
}

void Host::setSearchPaths(const std::vector<std::string>& paths) {
    impl_->searchPaths = paths;
}

Plugin* Host::loadPlugin(const std::string& path) {
    if (!fs::exists(path)) {
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
    if (count == 0) {
        std::cerr << "[CLAP] No plugins found in: " << path << std::endl;
        return nullptr;
    }

    // 最小限の clap_host — 必要に応じて拡張
    clap_host host{};
    Plugin* first = nullptr;

    for (uint32_t i = 0; i < count; ++i) {
        const auto* cd = entry->get_plugin_descriptor(i);
        if (!cd) continue;

        PluginDescriptor pd = makeDescriptor(cd);
        const auto* cp = entry->create_plugin(&host, i);
        if (!cp) continue;

        auto* instance = new PluginInstance(cp, pd, entry);
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

void Host::unloadAll() {
    for (auto* p : plugins_) {
        p->destroy();
        delete p;
    }
    plugins_.clear();
    impl_->libraries.clear();
}

std::vector<std::string> Host::scanPlugins() {
    std::vector<std::string> found;
    for (const auto& searchPath : impl_->searchPaths) {
        try {
            if (!fs::exists(searchPath)) continue;
            for (const auto& entry : fs::recursive_directory_iterator(searchPath)) {
                if (entry.path().extension() == ".clap" ||
                    entry.path().extension() == ".dll" ||
                    entry.path().extension() == ".so") {
                    found.push_back(entry.path().string());
                }
            }
        } catch (...) {}
    }
    return found;
}

} // namespace clap
