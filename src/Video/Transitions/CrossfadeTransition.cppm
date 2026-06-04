module;

module Video.Transitions.CrossfadeTransition;

import NLE.Core;
import Video.TransitionFactory;

namespace ArtifactCore {

namespace {

struct CrossfadeRegistrar {
    CrossfadeRegistrar()
    {
        TransitionFactory::instance().registerCreator(TransitionKind::Crossfade, []() {
            return new CrossfadeTransition();
        });
    }
};

static CrossfadeRegistrar registrar;

}

}