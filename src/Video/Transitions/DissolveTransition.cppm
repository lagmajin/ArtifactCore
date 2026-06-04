module;

module Video.Transitions.DissolveTransition;

import NLE.Core;
import Video.TransitionFactory;

namespace ArtifactCore {

namespace {

struct DissolveRegistrar {
    DissolveRegistrar()
    {
        TransitionFactory::instance().registerCreator(TransitionKind::Dissolve, []() {
            return new DissolveTransition();
        });
    }
};

static DissolveRegistrar registrar;

}

}