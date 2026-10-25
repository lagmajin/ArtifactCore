module;
#include <algorithm>
#include <array>
#include <cmath>

module Audio.Spatial.SpeakerLayout;

import Audio.Segment;
import Audio.Spatial.Math;

namespace ArtifactCore::Audio::Spatial {
namespace {

constexpr float kDegreesToRadians = 0.01745329251994329577f;

Vec3 unitDirection(float azimuthDegrees, float elevationDegrees) {
    const float azimuth = azimuthDegrees * kDegreesToRadians;
    const float elevation = elevationDegrees * kDegreesToRadians;
    const float horizontal = std::cos(elevation);
    return {std::sin(azimuth) * horizontal,
            std::sin(elevation),
            -std::cos(azimuth) * horizontal};
}

float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

float length(Vec3 value) {
    return std::sqrt(std::max(0.0f, dot(value, value)));
}

Vec3 normalized(Vec3 value) {
    const float valueLength = length(value);
    return valueLength > 1e-6f ? Vec3{value.x / valueLength, value.y / valueLength,
                                      value.z / valueLength}
                              : Vec3{0.0f, 0.0f, -1.0f};
}

void addSpeaker(SpeakerLayoutDescriptor& descriptor, int channel, float azimuth,
                float elevation, bool receivesObjectPanning = true) {
    if (channel < 0 || channel >= kMaxSpatialSpeakerChannels) return;
    descriptor.speakers[channel] = {channel, azimuth, elevation, receivesObjectPanning};
}

bool normalizeGains(SpeakerGains& gains) {
    float energy = 0.0f;
    for (int i = 0; i < gains.channelCount; ++i) energy += gains.values[i] * gains.values[i];
    if (!(energy > 1e-12f) || !std::isfinite(energy)) return false;
    const float inverse = 1.0f / std::sqrt(energy);
    for (int i = 0; i < gains.channelCount; ++i) gains.values[i] *= inverse;
    return true;
}

} // namespace

SpeakerLayoutDescriptor speakerLayout(AudioChannelLayout layout) {
    SpeakerLayoutDescriptor descriptor;
    switch (layout) {
    case AudioChannelLayout::Mono:
        descriptor.channelCount = 1;
        addSpeaker(descriptor, 0, 0.0f, 0.0f);
        break;
    case AudioChannelLayout::Surround51:
        descriptor.channelCount = 6;
        addSpeaker(descriptor, 0, -30.0f, 0.0f);
        addSpeaker(descriptor, 1, 30.0f, 0.0f);
        addSpeaker(descriptor, 2, 0.0f, 0.0f);
        addSpeaker(descriptor, 3, 0.0f, 0.0f, false); // LFE
        addSpeaker(descriptor, 4, -110.0f, 0.0f);
        addSpeaker(descriptor, 5, 110.0f, 0.0f);
        break;
    case AudioChannelLayout::Surround71:
    case AudioChannelLayout::Surround714:
        descriptor.channelCount = layout == AudioChannelLayout::Surround714 ? 12 : 8;
        addSpeaker(descriptor, 0, -30.0f, 0.0f);
        addSpeaker(descriptor, 1, 30.0f, 0.0f);
        addSpeaker(descriptor, 2, 0.0f, 0.0f);
        addSpeaker(descriptor, 3, 0.0f, 0.0f, false); // LFE
        addSpeaker(descriptor, 4, -90.0f, 0.0f);
        addSpeaker(descriptor, 5, 90.0f, 0.0f);
        addSpeaker(descriptor, 6, -150.0f, 0.0f);
        addSpeaker(descriptor, 7, 150.0f, 0.0f);
        if (layout == AudioChannelLayout::Surround714) {
            addSpeaker(descriptor, 8, -45.0f, 45.0f);
            addSpeaker(descriptor, 9, 45.0f, 45.0f);
            addSpeaker(descriptor, 10, -135.0f, 45.0f);
            addSpeaker(descriptor, 11, 135.0f, 45.0f);
        }
        break;
    case AudioChannelLayout::Stereo:
    default:
        descriptor.channelCount = 2;
        addSpeaker(descriptor, 0, -30.0f, 0.0f);
        addSpeaker(descriptor, 1, 30.0f, 0.0f);
        break;
    }
    return descriptor;
}

SpeakerGains calculateSpeakerGains(AudioChannelLayout layout, Vec3 direction) {
    const SpeakerLayoutDescriptor descriptor = speakerLayout(layout);
    SpeakerGains result;
    result.channelCount = descriptor.channelCount;
    direction = normalized(direction);

    // Select a valid 3D VBAP triplet. The determinant test rejects coplanar
    // base triangles, which leaves horizontal layouts to the pair fallback.
    float bestMinimum = -1.0f;
    std::array<float, kMaxSpatialSpeakerChannels> best{};
    for (int a = 0; a < descriptor.channelCount; ++a) {
        if (!descriptor.speakers[a].receivesObjectPanning) continue;
        const Vec3 va = unitDirection(descriptor.speakers[a].azimuthDegrees,
                                      descriptor.speakers[a].elevationDegrees);
        for (int b = a + 1; b < descriptor.channelCount; ++b) {
            if (!descriptor.speakers[b].receivesObjectPanning) continue;
            const Vec3 vb = unitDirection(descriptor.speakers[b].azimuthDegrees,
                                          descriptor.speakers[b].elevationDegrees);
            for (int c = b + 1; c < descriptor.channelCount; ++c) {
                if (!descriptor.speakers[c].receivesObjectPanning) continue;
                const Vec3 vc = unitDirection(descriptor.speakers[c].azimuthDegrees,
                                              descriptor.speakers[c].elevationDegrees);
                const float determinant = dot(va, cross(vb, vc));
                if (std::abs(determinant) < 1e-5f) continue;
                const float ga = dot(direction, cross(vb, vc)) / determinant;
                const float gb = dot(direction, cross(vc, va)) / determinant;
                const float gc = dot(direction, cross(va, vb)) / determinant;
                const float minimum = std::min({ga, gb, gc});
                if (minimum >= -1e-5f && minimum > bestMinimum) {
                    best.fill(0.0f);
                    best[a] = std::max(0.0f, ga);
                    best[b] = std::max(0.0f, gb);
                    best[c] = std::max(0.0f, gc);
                    bestMinimum = minimum;
                }
            }
        }
    }
    if (bestMinimum >= 0.0f) {
        result.values = best;
        if (normalizeGains(result)) return result;
    }

    // Horizontal pair VBAP handles stereo, 5.1 and directions outside an
    // incomplete 3D dome. It intentionally never feeds LFE.
    bestMinimum = -1.0f;
    for (int a = 0; a < descriptor.channelCount; ++a) {
        if (!descriptor.speakers[a].receivesObjectPanning) continue;
        const Vec3 va = unitDirection(descriptor.speakers[a].azimuthDegrees, 0.0f);
        for (int b = a + 1; b < descriptor.channelCount; ++b) {
            if (!descriptor.speakers[b].receivesObjectPanning) continue;
            const Vec3 vb = unitDirection(descriptor.speakers[b].azimuthDegrees, 0.0f);
            const float determinant = va.x * vb.z - vb.x * va.z;
            if (std::abs(determinant) < 1e-5f) continue;
            const float ga = (direction.x * vb.z - vb.x * direction.z) / determinant;
            const float gb = (va.x * direction.z - direction.x * va.z) / determinant;
            const float minimum = std::min(ga, gb);
            if (minimum >= -1e-5f && minimum > bestMinimum) {
                best.fill(0.0f);
                best[a] = std::max(0.0f, ga);
                best[b] = std::max(0.0f, gb);
                bestMinimum = minimum;
            }
        }
    }
    if (bestMinimum >= 0.0f) {
        result.values = best;
        result.usedFallback = true;
        if (normalizeGains(result)) return result;
    }

    int nearest = 0;
    float nearestDot = -2.0f;
    for (int channel = 0; channel < descriptor.channelCount; ++channel) {
        if (!descriptor.speakers[channel].receivesObjectPanning) continue;
        const auto speaker = descriptor.speakers[channel];
        const float candidate = dot(direction,
            unitDirection(speaker.azimuthDegrees, speaker.elevationDegrees));
        if (candidate > nearestDot) {
            nearest = channel;
            nearestDot = candidate;
        }
    }
    result.values[nearest] = 1.0f;
    result.usedFallback = true;
    return result;
}

} // namespace ArtifactCore::Audio::Spatial
