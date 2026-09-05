module;
#include <array>
#include <cstdint>

export module Audio.Spatial.SpeakerLayout;

import Audio.Segment;
import Audio.Spatial.Math;

export namespace ArtifactCore::Audio::Spatial {

constexpr int kMaxSpatialSpeakerChannels = 12;

struct SpeakerDescriptor {
    int channelIndex = -1;
    float azimuthDegrees = 0.0f;
    float elevationDegrees = 0.0f;
    bool receivesObjectPanning = false;
};

struct SpeakerLayoutDescriptor {
    std::array<SpeakerDescriptor, kMaxSpatialSpeakerChannels> speakers{};
    int channelCount = 0;
};

struct SpeakerGains {
    std::array<float, kMaxSpatialSpeakerChannels> values{};
    int channelCount = 0;
    bool usedFallback = false;
};

// The descriptor keeps speaker direction separate from PCM order. LFE is
// deliberately excluded from object panning and must be fed by an explicit
// bass-management/send stage.
SpeakerLayoutDescriptor speakerLayout(AudioChannelLayout layout);
SpeakerGains calculateSpeakerGains(AudioChannelLayout layout, Vec3 direction);

} // namespace ArtifactCore::Audio::Spatial
