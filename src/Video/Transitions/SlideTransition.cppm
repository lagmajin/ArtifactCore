module;

module Video.Transitions.SlideTransition;

import NLE.Core;
import Video.TransitionFactory;

namespace ArtifactCore {

namespace {

struct SlideRegistrar {
    SlideRegistrar()
    {
        TransitionFactory::instance().registerCreator(TransitionKind::Slide, []() {
            return new SlideTransition();
        });
    }
};

static SlideRegistrar registrar;

}

}