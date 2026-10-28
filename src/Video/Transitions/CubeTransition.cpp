module;

module Video.Transitions.CubeTransition;

import NLE.Core;
import Video.TransitionFactory;

namespace ArtifactCore {

namespace {

struct CubeRegistrar {
    CubeRegistrar() {
        TransitionFactory::instance().registerCreator(TransitionKind::Cube, []() {
            return new CubeTransition();
        });
    }
};

static CubeRegistrar registrar;

}

}

namespace ArtifactCore {
void registerCubeTransition()
{
    TransitionFactory::instance().registerCreator(TransitionKind::Cube, []() {
        return new CubeTransition();
    });
}
}
