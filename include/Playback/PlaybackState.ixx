module;
#include <utility>
export module Playback.State;

#include <wobjectdefs.h>

export namespace ArtifactCore {

 /**
  * @brief Playback states for engine and controllers
  */
 enum class PlaybackState {
  Stopped,
  Playing,
  Paused,
  Buffering,
  Error
 };

}

W_REGISTER_ARGTYPE(ArtifactCore::PlaybackState)
