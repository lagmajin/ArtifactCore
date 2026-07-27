module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

module Render.VolumeModifier;

import Core.Parallel;

namespace ArtifactCore::RayTrace {

VolumeModifier::VolumeModifier(ModifierKind kind) {
    props_.kind = kind;
}

void VolumeModifier::setProperties(const ModifierProperties& props) {
    props_ = props;
}

void VolumeModifier::apply(VolumeScalarField& field) const noexcept {
    switch (props_.kind) {
    case ModifierKind::TurbulenceDisplace: applyTurbulenceDisplace(field); break;
    case ModifierKind::Smooth: applySmooth(field); break;
    case ModifierKind::Clamp: applyClamp(field); break;
    case ModifierKind::Multiply: applyMultiply(field); break;
    case ModifierKind::Add: applyAdd(field); break;
    default: break;
    }
}

void VolumeModifier::applyInPlace(VolumeScalarField& density, VolumeScalarField& /*temperature*/) const noexcept {
    apply(density);
}

VolumeModifier VolumeModifier::createTurbulenceDisplace(float strength, const NoiseSettings& noise) {
    VolumeModifier mod(ModifierKind::TurbulenceDisplace);
    ModifierProperties props;
    props.strength = strength;
    props.noise = noise;
    mod.setProperties(props);
    return mod;
}

VolumeModifier VolumeModifier::createSmooth(int /*iterations*/) {
    VolumeModifier mod(ModifierKind::Smooth);
    return mod;
}

void VolumeModifier::applyTurbulenceDisplace(VolumeScalarField& field) const noexcept {
    if (field.empty()) return;
    const auto res = field.resolution;
    const float invW = res.width > 1 ? 1.0f / static_cast<float>(res.width) : 1.0f;
    const float invH = res.height > 1 ? 1.0f / static_cast<float>(res.height) : 1.0f;
    const float invD = res.depth > 1 ? 1.0f / static_cast<float>(res.depth) : 1.0f;

    std::vector<float> original(res.cellCount());
    const auto sliceStride = static_cast<std::size_t>(res.width) * res.height;
    Parallel::For(0, res.depth, res.width * res.height, [&](int z) {
        for (int y = 0; y < res.height; ++y) {
            const auto rowBase = static_cast<std::size_t>(z) * sliceStride +
                                 static_cast<std::size_t>(y) * res.width;
            for (int x = 0; x < res.width; ++x) {
                const auto index = rowBase + static_cast<std::size_t>(x);
                original[index] = field.data[index];
            }
        }
    });

    Parallel::For(0, res.depth, res.width * res.height, [&](int z) {
        for (int y = 0; y < res.height; ++y) {
            const auto rowBase = static_cast<std::size_t>(z) * sliceStride +
                                 static_cast<std::size_t>(y) * res.width;
            for (int x = 0; x < res.width; ++x) {
                const float fx = static_cast<float>(x) * invW + props_.center.x;
                const float fy = static_cast<float>(y) * invH + props_.center.y;
                const float fz = static_cast<float>(z) * invD + props_.center.z;

                const float noiseVal = turbulence3D(fx, fy, fz, props_.noise);
                const float displacement = (noiseVal - 0.5f) * 2.0f * props_.strength * invW;

                const float sampleX = static_cast<float>(x) + displacement;
                const float sampleY = static_cast<float>(y) + displacement * 0.5f;
                const float sampleZ = static_cast<float>(z) + displacement * 0.3f;

                const int ix = std::clamp(static_cast<int>(sampleX), 0, res.width - 1);
                const int iy = std::clamp(static_cast<int>(sampleY), 0, res.height - 1);
                const int iz = std::clamp(static_cast<int>(sampleZ), 0, res.depth - 1);

                field.data[rowBase + static_cast<std::size_t>(x)] =
                    original[static_cast<std::size_t>(iz) * sliceStride +
                             static_cast<std::size_t>(iy) * res.width +
                             static_cast<std::size_t>(ix)];
            }
        }
    });
}

void VolumeModifier::applySmooth(VolumeScalarField& field) const noexcept {
    if (field.empty()) return;
    const auto res = field.resolution;

    std::vector<float> original(res.cellCount());
    const auto sliceStride = static_cast<std::size_t>(res.width) * res.height;
    Parallel::For(0, res.depth, res.width * res.height, [&](int z) {
        for (int y = 0; y < res.height; ++y) {
            const auto rowBase = static_cast<std::size_t>(z) * sliceStride +
                                 static_cast<std::size_t>(y) * res.width;
            for (int x = 0; x < res.width; ++x) {
                const auto index = rowBase + static_cast<std::size_t>(x);
                original[index] = field.data[index];
            }
        }
    });

    Parallel::For(0, res.depth, res.width * res.height, [&](int z) {
        for (int y = 0; y < res.height; ++y) {
            const auto rowBase = static_cast<std::size_t>(z) * sliceStride +
                                 static_cast<std::size_t>(y) * res.width;
            for (int x = 0; x < res.width; ++x) {
                float sum = 0.0f;
                int count = 0;
                for (int dz = -1; dz <= 1; ++dz) {
                    const int nz = z + dz;
                    if (nz < 0 || nz >= res.depth) continue;
                    for (int dy = -1; dy <= 1; ++dy) {
                        const int ny = y + dy;
                        if (ny < 0 || ny >= res.height) continue;
                        for (int dx = -1; dx <= 1; ++dx) {
                            const int nx = x + dx;
                            if (nx < 0 || nx >= res.width) continue;
                            sum += original[static_cast<std::size_t>(nz) * sliceStride +
                                             static_cast<std::size_t>(ny) * res.width +
                                             static_cast<std::size_t>(nx)];
                            ++count;
                        }
                    }
                }
                field.data[rowBase + static_cast<std::size_t>(x)] =
                    sum / static_cast<float>(count);
            }
        }
    });
}

void VolumeModifier::applyClamp(VolumeScalarField& field) const noexcept {
    if (field.empty()) return;
    const auto res = field.resolution;
    const auto sliceStride = static_cast<std::size_t>(res.width) * res.height;
    Parallel::For(0, res.depth, res.width * res.height, [&](int z) {
        for (int y = 0; y < res.height; ++y) {
            const auto rowBase = static_cast<std::size_t>(z) * sliceStride +
                                 static_cast<std::size_t>(y) * res.width;
            for (int x = 0; x < res.width; ++x) {
                const auto index = rowBase + static_cast<std::size_t>(x);
                field.data[index] = std::clamp(field.data[index], props_.minValue, props_.maxValue);
            }
        }
    });
}

void VolumeModifier::applyMultiply(VolumeScalarField& field) const noexcept {
    if (field.empty()) return;
    const auto res = field.resolution;
    const auto sliceStride = static_cast<std::size_t>(res.width) * res.height;
    Parallel::For(0, res.depth, res.width * res.height, [&](int z) {
        for (int y = 0; y < res.height; ++y) {
            const auto rowBase = static_cast<std::size_t>(z) * sliceStride +
                                 static_cast<std::size_t>(y) * res.width;
            for (int x = 0; x < res.width; ++x) {
                field.data[rowBase + static_cast<std::size_t>(x)] *= props_.multiplier;
            }
        }
    });
}

void VolumeModifier::applyAdd(VolumeScalarField& field) const noexcept {
    if (field.empty()) return;
    const auto res = field.resolution;
    const auto sliceStride = static_cast<std::size_t>(res.width) * res.height;
    Parallel::For(0, res.depth, res.width * res.height, [&](int z) {
        for (int y = 0; y < res.height; ++y) {
            const auto rowBase = static_cast<std::size_t>(z) * sliceStride +
                                 static_cast<std::size_t>(y) * res.width;
            for (int x = 0; x < res.width; ++x) {
                field.data[rowBase + static_cast<std::size_t>(x)] += props_.addend;
            }
        }
    });
}

void VolumeModifierStack::addModifier(ModifierKind kind, const ModifierProperties& props) {
    VolumeModifier mod(kind);
    mod.setProperties(props);
    modifiers_.push_back(std::move(mod));
}

void VolumeModifierStack::addModifier(const VolumeModifier& modifier) {
    modifiers_.push_back(modifier);
}

void VolumeModifierStack::applyAll(VolumeScalarField& field) const noexcept {
    for (const auto& mod : modifiers_) {
        mod.apply(field);
    }
}

void VolumeModifierStack::clear() {
    modifiers_.clear();
}

} // namespace ArtifactCore::RayTrace
