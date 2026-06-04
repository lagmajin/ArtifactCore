module;

module Video.Transitions.LinearWipeTransition;

import NLE.Core;
import Video.TransitionFactory;

namespace ArtifactCore {

namespace {

struct LinearWipeRegistrar {
    LinearWipeRegistrar() {
        TransitionFactory::instance().registerCreator(TransitionKind::LinearWipe, []() {
            return new LinearWipeTransition();
        });
    }
};

static LinearWipeRegistrar registrar;

}

}