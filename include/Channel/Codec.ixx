module;
#include <utility>

export module Codec;

// Video encoder interfaces are owned by ArtifactCoreVideo.  Do not re-export
// them from this base target: that would introduce a reverse module edge.
