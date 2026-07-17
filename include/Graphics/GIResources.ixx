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

struct GISettings {
    float radius = 1.0f;
    float intensity = 1.0f;
    float temporalWeight = 0.9f;
    std::uint32_t sampleCount = 8;
    bool enabled = true;
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
