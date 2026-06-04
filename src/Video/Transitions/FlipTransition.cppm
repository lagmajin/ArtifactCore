module;

module Video.Transitions.FlipTransition;

import NLE.Core;
import Video.TransitionFactory;

namespace ArtifactCore {

namespace {

struct FlipRegistrar {
    FlipRegistrar() {
        TransitionFactory::instance().registerCreator(TransitionKind::Flip, []() {
            return new FlipTransition();
        });
    }
};

static FlipRegistrar registrar;

}

}