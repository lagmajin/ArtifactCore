module;

module Video.Transitions.DoorsTransition;

import NLE.Core;
import Video.TransitionFactory;

namespace ArtifactCore {

namespace {

struct DoorsRegistrar {
    DoorsRegistrar() {
        TransitionFactory::instance().registerCreator(TransitionKind::Doors, []() {
            return new DoorsTransition();
        });
    }
};

static DoorsRegistrar registrar;

}

}