module;
#include <utility>
#include "../Define/DllExportMacro.hpp"

export module Audio.DownMixer;

import Audio.Segment;

export namespace ArtifactCore {

 /**
  * @brief Handles conversion from multi-channel audio to Stereo or Mono.
  * 
  * This component is essential for mixed-format projects where 5.1ch or 7.1ch 
  * assets need to be played back on standard stereo output devices.
  */
 class LIBRARY_DLL_API AudioDownMixer {
 public:
  AudioDownMixer();
  ~AudioDownMixer();

  /**
   * @brief Set the desired output layout (usually Mono or Stereo)
   */
  void setTargetLayout(AudioChannelLayout target);
  AudioChannelLayout targetLayout() const;

  /**
   * @brief Mixing levels for multi-channel elements
   */
  void setCenterMixLevel(float level = 0.7071f); // -3dB default
  void setLFEMixLevel(float level = 0.7071f);
  void setSurroundMixLevel(float level = 0.5f); // -6dB default

  /**
   * @brief Process and return the downmixed segment
   */
  AudioSegment process(const AudioSegment& source) const;

 private:
  struct Impl;
  Impl* impl_;
 };

};
