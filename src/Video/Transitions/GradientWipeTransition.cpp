module;

module Video.Transitions.GradientWipeTransition;

import NLE.Core;
import Video.TransitionFactory;

namespace ArtifactCore {

namespace {

struct GradientWipeRegistrar {
    GradientWipeRegistrar() {
        TransitionFactory::instance().registerCreator(TransitionKind::GradientWipe, []() {
            return new GradientWipeTransition();
        });
    }
};

static GradientWipeRegistrar registrar;

}

}
