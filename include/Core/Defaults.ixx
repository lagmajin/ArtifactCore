module;
#include <cstddef>

export module Core.Defaults;

export namespace ArtifactCore {

 // Default composition/project values
 constexpr int kDefaultWidth = 1920;
 constexpr int kDefaultHeight = 1080;
 constexpr double kDefaultFrameRate = 30.0;
 constexpr int kDefaultBitrate = 8000;
 constexpr int kDefaultDurationFrames = 100;

 // Audio defaults
 constexpr int kDefaultSampleRate = 48000;
 constexpr int kDefaultAudioChannels = 2;
 constexpr int kAudioRingBufferCapacity = kDefaultSampleRate * 8; // 8 seconds

 // Render defaults
 constexpr int kDefaultRenderBitrateKbps = 8000;
 constexpr int kDefaultStartFrame = 0;

}
