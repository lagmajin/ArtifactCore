module;

module Video.Transitions.LightLeakTransition;

import NLE.Core;
import Video.TransitionFactory;

namespace ArtifactCore {

namespace {

struct LightLeakRegistrar {
    LightLeakRegistrar() {
        TransitionFactory::instance().registerCreator(TransitionKind::LightLeak, []() {
            return new LightLeakTransition();
        });
    }
};

static LightLeakRegistrar registrar;

}

}

namespace ArtifactCore {
void registerLightLeakTransition()
{
    TransitionFactory::instance().registerCreator(TransitionKind::LightLeak, []() {
        return new LightLeakTransition();
    });
}
}
