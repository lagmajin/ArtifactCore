module;
#include <cstdint>
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

}
