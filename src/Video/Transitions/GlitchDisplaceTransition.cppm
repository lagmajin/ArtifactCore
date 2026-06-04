module;

module Video.Transitions.GlitchDisplaceTransition;

import NLE.Core;
import Video.TransitionFactory;

namespace ArtifactCore {

namespace {

struct GlitchDisplaceRegistrar {
    GlitchDisplaceRegistrar()
    {
        TransitionFactory::instance().registerCreator(TransitionKind::GlitchDisplace, []() {
            return new GlitchDisplaceTransition();
        });
    }
};

static GlitchDisplaceRegistrar registrar;

}

}