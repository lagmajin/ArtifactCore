module;

module Video.Transitions.RadialWipeTransition;

import NLE.Core;
import Video.TransitionFactory;

namespace ArtifactCore {

namespace {

struct RadialWipeRegistrar {
    RadialWipeRegistrar() {
        TransitionFactory::instance().registerCreator(TransitionKind::RadialWipe, []() {
            return new RadialWipeTransition();
        });
    }
};

static RadialWipeRegistrar registrar;

}

}