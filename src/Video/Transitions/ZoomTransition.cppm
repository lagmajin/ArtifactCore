module;

module Video.Transitions.ZoomTransition;

import NLE.Core;
import Video.TransitionFactory;

namespace ArtifactCore {

namespace {

struct ZoomRegistrar {
    ZoomRegistrar()
    {
        TransitionFactory::instance().registerCreator(TransitionKind::Zoom, []() {
            return new ZoomTransition();
        });
    }
};

static ZoomRegistrar registrar;

}

}