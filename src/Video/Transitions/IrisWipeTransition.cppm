module;

module Video.Transitions.IrisWipeTransition;

import NLE.Core;
import Video.TransitionFactory;

namespace ArtifactCore {

namespace {

struct IrisWipeRegistrar {
    IrisWipeRegistrar() {
        TransitionFactory::instance().registerCreator(TransitionKind::IrisWipe, []() {
            return new IrisWipeTransition();
        });
    }
};

static IrisWipeRegistrar registrar;

}

}
