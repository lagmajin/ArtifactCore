module;

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <variant>

export module Video.CpuFrameView;

import Video.VideoFrame;

export namespace ArtifactCore {

class CpuFrameView {
public:
    explicit CpuFrameView(const DecodedVideoFrame& frame)
        : frame_(frame)
    {
    }

    const std::uint8_t* scanline(int y) const
    {
        return cpu().bytes.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(cpu().strideBytes);
    }

    const std::uint8_t* data() const
    {
        return cpu().bytes.data();
    }

    int width() const { return cpu().meta.width; }
    int height() const { return cpu().meta.height; }
    int strideBytes() const { return cpu().strideBytes; }
    bool isValid() const { return cpu().isValid(); }

    static std::uint8_t pixel(const std::uint8_t* scanline, int x, int channel)
    {
        return scanline[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(channel)];
    }

    static void setPixel(std::uint8_t* scanline, int x, int channel, std::uint8_t value)
    {
        scanline[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(channel)] = value;
    }

private:
    const CpuVideoFrame& cpu() const
    {
        assert(std::holds_alternative<CpuVideoFrame>(frame_));
        return std::get<CpuVideoFrame>(frame_);
    }

    const DecodedVideoFrame& frame_;
};

}
