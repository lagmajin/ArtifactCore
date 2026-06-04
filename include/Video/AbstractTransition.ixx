module;

#include <cstdint>
#include "../Define/DllExportMacro.hpp"
#include <QVariantMap>

export module Video.AbstractTransition;

import NLE.Core;
import Video.VideoFrame;

export namespace ArtifactCore {

using namespace ArtifactCore::NLE;

struct TransitionContext {
    double progress = 0.0;
    int frameIndex = 0;
    double fps = 30.0;
    int width = 1920;
    int height = 1080;
    const Transition* transition = nullptr;
};

class LIBRARY_DLL_API AbstractTransition {
public:
    virtual ~AbstractTransition() = default;

    virtual const char* name() const = 0;
    virtual TransitionKind kind() const = 0;

    virtual bool initialize(const TransitionContext& ctx) { return true; }

    virtual void process(const DecodedVideoFrame& leftFrame,
                         const DecodedVideoFrame& rightFrame,
                         const TransitionContext& ctx) = 0;

    virtual bool validateParameters(const QVariantMap&) const { return true; }
};

}
