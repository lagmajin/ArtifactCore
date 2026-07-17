module;
#include <cstdint>
#include <vector>
#include "../Define/DllExportMacro.hpp"

export module Graphics.GIResources;

export namespace ArtifactCore {

enum class GIResourceKind : std::uint8_t {
    Depth,
    Normal,
    Motion,
    DirectLighting,
    IndirectLighting,
    History
};

enum class GIMode : std::uint8_t {
    Disabled,
    Fast,
    Quality
};

struct GISettings {
    float radius = 1.0f;
    float intensity = 1.0f;
    float temporalWeight = 0.9f;
    std::uint32_t sampleCount = 8;
    float resolutionScale = 1.0f;
    GIMode mode = GIMode::Fast;
    bool enabled = true;

    static GISettings fast()
    {
        GISettings settings;
        settings.mode = GIMode::Fast;
        settings.sampleCount = 4;
        settings.radius = 0.75f;
        settings.temporalWeight = 0.85f;
        settings.resolutionScale = 0.5f;
        return settings;
    }

    static GISettings quality()
    {
        GISettings settings;
        settings.mode = GIMode::Quality;
        settings.sampleCount = 24;
        settings.radius = 1.5f;
        settings.temporalWeight = 0.94f;
        settings.resolutionScale = 1.0f;
        return settings;
    }

    static GISettings disabled()
    {
        GISettings settings;
        settings.mode = GIMode::Disabled;
        settings.enabled = false;
        settings.sampleCount = 0;
        return settings;
    }
};

struct GIResourceDescriptor {
    GIResourceKind kind = GIResourceKind::Depth;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t channels = 1;
};

enum class GIPassKind : std::uint8_t {
    Reconstruct,
    ScreenSpaceGather,
    DepthPyramid,
    BilateralDenoise,
    TemporalResolve,
    Composite
};

struct GIPassDescriptor {
    GIPassKind kind = GIPassKind::Reconstruct;
    GIResourceKind input = GIResourceKind::Depth;
    GIResourceKind output = GIResourceKind::IndirectLighting;
    std::uint32_t sampleCount = 0;
    bool requiresHistory = false;
};

class LIBRARY_DLL_API GIExecutionPlan {
public:
    static GIExecutionPlan forSettings(const GISettings& settings)
    {
        GIExecutionPlan plan;
        if (!settings.enabled || settings.mode == GIMode::Disabled) return plan;

        plan.passes_.push_back({GIPassKind::Reconstruct,
                                GIResourceKind::Depth,
                                GIResourceKind::Normal,
                                0,
                                false});
        if (settings.mode == GIMode::Quality) {
            plan.passes_.push_back({GIPassKind::DepthPyramid,
                                    GIResourceKind::Depth,
                                    GIResourceKind::Depth,
                                    0,
                                    false});
        }
        plan.passes_.push_back({GIPassKind::ScreenSpaceGather,
                                GIResourceKind::Normal,
                                GIResourceKind::IndirectLighting,
                                settings.sampleCount,
                                false});

        if (settings.mode == GIMode::Quality) {
            plan.passes_.push_back({GIPassKind::BilateralDenoise,
                                    GIResourceKind::IndirectLighting,
                                    GIResourceKind::IndirectLighting,
                                    0,
                                    false});
        }

        plan.passes_.push_back({GIPassKind::TemporalResolve,
                                GIResourceKind::IndirectLighting,
                                GIResourceKind::History,
                                0,
                                true});
        plan.passes_.push_back({GIPassKind::Composite,
                                GIResourceKind::History,
                                GIResourceKind::DirectLighting,
                                0,
                                true});
        return plan;
    }

    const std::vector<GIPassDescriptor>& passes() const noexcept { return passes_; }
    bool empty() const noexcept { return passes_.empty(); }

    bool contains(const GIPassKind kind) const noexcept
    {
        for (const auto& pass : passes_) {
            if (pass.kind == kind) return true;
        }
        return false;
    }

    bool requiresHistory() const noexcept
    {
        for (const auto& pass : passes_) {
            if (pass.requiresHistory) return true;
        }
        return false;
    }

    std::uint32_t gatherSampleCount() const noexcept
    {
        for (const auto& pass : passes_) {
            if (pass.kind == GIPassKind::ScreenSpaceGather) return pass.sampleCount;
        }
        return 0;
    }

private:
    std::vector<GIPassDescriptor> passes_;
};

class LIBRARY_DLL_API GIFrameResources {
public:
    void resize(std::uint32_t width, std::uint32_t height)
    {
        width_ = width;
        height_ = height;
        ++generation_;
    }

    bool valid() const noexcept { return width_ != 0 && height_ != 0; }
    std::uint32_t width() const noexcept { return width_; }
    std::uint32_t height() const noexcept { return height_; }
    std::uint64_t generation() const noexcept { return generation_; }

    std::uint32_t workingWidth(const GISettings& settings) const noexcept
    {
        return scaledDimension(width_, settings.resolutionScale);
    }

    std::uint32_t workingHeight(const GISettings& settings) const noexcept
    {
        return scaledDimension(height_, settings.resolutionScale);
    }

    GIResourceDescriptor descriptor(GIResourceKind kind) const noexcept
    {
        const auto channels = kind == GIResourceKind::Normal ||
                              kind == GIResourceKind::Motion ||
                              kind == GIResourceKind::DirectLighting ||
                              kind == GIResourceKind::IndirectLighting ||
                              kind == GIResourceKind::History ? 4u : 1u;
        return {kind, width_, height_, channels};
    }

private:
    static std::uint32_t scaledDimension(const std::uint32_t value, const float scale) noexcept
    {
        if (value == 0 || scale <= 0.0f) return 0;
        const auto scaled = static_cast<std::uint32_t>(static_cast<float>(value) * scale);
        return scaled == 0 ? 1 : scaled;
    }

    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::uint64_t generation_ = 0;
};

class LIBRARY_DLL_API GIAccumulationState {
public:
    void reset() noexcept { historyValid_ = false; ++generation_; }

    void beginFrame(std::uint64_t frame, bool cameraCut = false) noexcept
    {
        if (cameraCut || (frame != 0 && frame != lastFrame_ + 1)) {
            historyValid_ = false;
            ++generation_;
        }
        lastFrame_ = frame;
    }

    void commitHistory() noexcept { historyValid_ = true; }
    bool historyValid() const noexcept { return historyValid_; }
    std::uint64_t lastFrame() const noexcept { return lastFrame_; }
    std::uint64_t generation() const noexcept { return generation_; }

private:
    std::uint64_t lastFrame_ = 0;
    std::uint64_t generation_ = 0;
    bool historyValid_ = false;
};

class LIBRARY_DLL_API GIFrameContext {
public:
    explicit GIFrameContext(GISettings settings = GISettings::fast())
        : settings_(settings), plan_(GIExecutionPlan::forSettings(settings))
    {}

    void configure(GISettings settings)
    {
        settings_ = settings;
        plan_ = GIExecutionPlan::forSettings(settings_);
        accumulation_.reset();
    }

    void resize(std::uint32_t width, std::uint32_t height)
    {
        if (resources_.width() != width || resources_.height() != height) {
            resources_.resize(width, height);
            accumulation_.reset();
        }
    }

    void beginFrame(std::uint64_t frame, bool cameraCut = false) noexcept
    {
        accumulation_.beginFrame(frame, cameraCut);
    }

    void commitHistory() noexcept { accumulation_.commitHistory(); }
    bool historyReady() const noexcept { return accumulation_.historyValid(); }
    const GISettings& settings() const noexcept { return settings_; }
    const GIExecutionPlan& plan() const noexcept { return plan_; }
    const GIFrameResources& resources() const noexcept { return resources_; }
    GIFrameResources& resources() noexcept { return resources_; }
    const GIAccumulationState& accumulation() const noexcept { return accumulation_; }

private:
    GISettings settings_;
    GIFrameResources resources_;
    GIAccumulationState accumulation_;
    GIExecutionPlan plan_;
};

}
