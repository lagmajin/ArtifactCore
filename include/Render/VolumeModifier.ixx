module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

export module Render.VolumeModifier;

import Render.Vector3D;
import Render.VolumeRenderer;
import Render.NoiseField;

export namespace ArtifactCore::RayTrace {

enum class ModifierKind : std::uint8_t {
    TurbulenceDisplace = 0,
    WindAdvection = 1,
    Vortex = 2,
    Stretch = 3,
    Smooth = 4,
    Clamp = 5,
    Multiply = 6,
    Add = 7,
    Remap = 8,
};

struct ModifierProperties {
    ModifierKind kind = ModifierKind::TurbulenceDisplace;
    float strength = 1.0f;
    float falloff = 0.0f;
    Vec3 axis{1.0f, 0.0f, 0.0f};
    Vec3 center{0.5f, 0.5f, 0.5f};
    NoiseSettings noise{};
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float multiplier = 1.0f;
    float addend = 0.0f;
};

class VolumeModifier {
public:
    explicit VolumeModifier(ModifierKind kind);

    void setProperties(const ModifierProperties& props);
    [[nodiscard]] const ModifierProperties& properties() const noexcept { return props_; }

    void apply(VolumeScalarField& field) const noexcept;
    void applyInPlace(VolumeScalarField& density, VolumeScalarField& temperature) const noexcept;

    static VolumeModifier createTurbulenceDisplace(float strength, const NoiseSettings& noise);
    static VolumeModifier createSmooth(int iterations);

private:
    ModifierProperties props_;

    void applyTurbulenceDisplace(VolumeScalarField& field) const noexcept;
    void applySmooth(VolumeScalarField& field) const noexcept;
    void applyClamp(VolumeScalarField& field) const noexcept;
    void applyMultiply(VolumeScalarField& field) const noexcept;
    void applyAdd(VolumeScalarField& field) const noexcept;
};

class VolumeModifierStack {
public:
    void addModifier(ModifierKind kind, const ModifierProperties& props = {});
    void addModifier(const VolumeModifier& modifier);

    void applyAll(VolumeScalarField& field) const noexcept;
    void clear();

    [[nodiscard]] std::size_t count() const noexcept { return modifiers_.size(); }

private:
    std::vector<VolumeModifier> modifiers_;
};

} // namespace ArtifactCore::RayTrace