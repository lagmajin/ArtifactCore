module;
#include <cstddef>
#include <cstdint>
#include <cassert>
#include <limits>
#include <optional>
#include <type_traits>

export module Image.ImageSurfaceView;

import Graphics.SurfaceColorContract;

export namespace ArtifactCore {

enum class SurfacePrecision : std::uint8_t {
    Float16,
    Float32,
};

struct ImageSurfaceView {
    const void* data = nullptr;
    int width = 0;
    int height = 0;
    std::size_t rowStride = 0;
    SurfacePrecision precision = SurfacePrecision::Float32;
    SurfaceColorDescriptor descriptor = SurfaceColorDescriptor::unknown();

    bool isValid() const noexcept {
        return data != nullptr && width > 0 && height > 0 && rowStride != 0;
    }
};

/// Non-owning 8-bit RGBA/BGRA image view for analysis and explicit CPU-side
/// boundaries. It is intentionally separate from ImageSurfaceView so GPU upload
/// APIs that consume float surfaces cannot mistake normalized bytes for floats.
struct ImageByteSurfaceView {
    const std::uint8_t* data = nullptr;
    int width = 0;
    int height = 0;
    std::size_t rowStride = 0;
    SurfaceColorDescriptor descriptor = SurfaceColorDescriptor::unknown();

    bool isValid() const noexcept {
        return data != nullptr && width > 0 && height > 0 && rowStride != 0;
    }
};

// Non-owning views: the buffer must outlive the view and keep its layout.
// A factory checks the declared layout, not the semantic contents of raw bytes.
template <typename Component>
struct ColorFloat4Channels {
    Component& r;
    Component& g;
    Component& b;
    Component& a;
};

template <SurfaceChannelOrder Order, bool Mutable>
class ColorFloat4View;

template <SurfaceChannelOrder Order, typename Component>
class ColorFloat4Row {
    static_assert(Order == SurfaceChannelOrder::RGBA || Order == SurfaceChannelOrder::BGRA);
    Component* data_;
    int width_;
    ColorFloat4Row(Component* data, int width) noexcept : data_(data), width_(width) {}
    template <SurfaceChannelOrder, bool> friend class ColorFloat4View;

public:
    ColorFloat4Channels<Component> operator[](int x) const noexcept {
        assert(x >= 0 && x < width_);
        Component* pixel = data_ + static_cast<std::size_t>(x)*4u;
        if constexpr (Order == SurfaceChannelOrder::RGBA)
            return {pixel[0], pixel[1], pixel[2], pixel[3]};
        else
            return {pixel[2], pixel[1], pixel[0], pixel[3]};
    }
};

template <SurfaceChannelOrder Order, bool Mutable>
class ColorFloat4View {
    static_assert(Order == SurfaceChannelOrder::RGBA || Order == SurfaceChannelOrder::BGRA);
    using Component = std::conditional_t<Mutable, float, const float>;
    Component* data_;
    int width_;
    int height_;
    std::size_t rowStride_;
    SurfaceColorDescriptor descriptor_;

    ColorFloat4View(Component* data, const ImageSurfaceView& surface) noexcept
        : data_(data), width_(surface.width), height_(surface.height),
          rowStride_(surface.rowStride), descriptor_(surface.descriptor) {}

    static bool accepts(const ImageSurfaceView& surface) noexcept {
        if (!surface.isValid() || surface.precision != SurfacePrecision::Float32 ||
            surface.descriptor.storage != SurfacePixelStorage::RGBA32Float ||
            surface.descriptor.channelOrder != Order ||
            reinterpret_cast<std::uintptr_t>(surface.data) % alignof(float) != 0 ||
            surface.rowStride % sizeof(float) != 0) return false;
        constexpr auto maxSize = (std::numeric_limits<std::size_t>::max)();
        if (static_cast<std::size_t>(surface.width) > maxSize/(4u*sizeof(float))) return false;
        const auto rowBytes = static_cast<std::size_t>(surface.width)*4u*sizeof(float);
        if (surface.rowStride < rowBytes) return false;
        const auto precedingRows = static_cast<std::size_t>(surface.height-1);
        if (precedingRows > (maxSize-rowBytes)/surface.rowStride) return false;
        const auto span = precedingRows*surface.rowStride+rowBytes;
        if (span > static_cast<std::size_t>((std::numeric_limits<std::ptrdiff_t>::max)())) return false;
        return reinterpret_cast<std::uintptr_t>(surface.data) <=
            (std::numeric_limits<std::uintptr_t>::max)()-span;
    }

public:
    static constexpr SurfaceChannelOrder channelOrder = Order;
    // No public/default construction and no conversion between RGBA/BGRA views.
    static std::optional<ColorFloat4View> tryCreate(const ImageSurfaceView& surface) noexcept
        requires (!Mutable) {
        if (!accepts(surface)) return std::nullopt;
        return ColorFloat4View(static_cast<const float*>(surface.data), surface);
    }
    static std::optional<ColorFloat4View> tryCreate(
        const ImageSurfaceView& surface, float* writableData) noexcept requires (Mutable) {
        // Writable access must come from the same buffer's mutable owner API.
        if (!accepts(surface) || writableData != surface.data) return std::nullopt;
        return ColorFloat4View(writableData, surface);
    }
    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }
    const SurfaceColorDescriptor& colorDescriptor() const noexcept { return descriptor_; }
    ColorFloat4Row<Order, Component> row(int y) const noexcept {
        assert(y >= 0 && y < height_);
        return {data_ + static_cast<std::size_t>(y)*(rowStride_/sizeof(float)), width_};
    }
};

using Rgba32FView = ColorFloat4View<SurfaceChannelOrder::RGBA, false>;
using Bgra32FView = ColorFloat4View<SurfaceChannelOrder::BGRA, false>;
using MutableRgba32FView = ColorFloat4View<SurfaceChannelOrder::RGBA, true>;
using MutableBgra32FView = ColorFloat4View<SurfaceChannelOrder::BGRA, true>;

static_assert(!std::is_convertible_v<Rgba32FView, Bgra32FView>);
static_assert(!std::is_convertible_v<MutableRgba32FView, MutableBgra32FView>);
static_assert(!std::is_default_constructible_v<MutableRgba32FView>);
static_assert(!std::is_convertible_v<Rgba32FView, MutableRgba32FView>);

// Dispatch once outside the pixel loop. Unknown/mismatched layouts are rejected;
// this function never relabels, converts, copies or allocates a pixel buffer.
template <typename Function>
[[nodiscard]] bool withMutableColorFloat4View(
    const ImageSurfaceView& surface, float* writableData, Function&& function) {
    if (auto rgba = MutableRgba32FView::tryCreate(surface, writableData)) {
        function(*rgba);
        return true;
    }
    if (auto bgra = MutableBgra32FView::tryCreate(surface, writableData)) {
        function(*bgra);
        return true;
    }
    return false;
}

}
