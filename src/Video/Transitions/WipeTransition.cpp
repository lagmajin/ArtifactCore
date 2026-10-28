module;

module Video.Transitions.WipeTransition;

import NLE.Core;
import Video.TransitionFactory;

namespace ArtifactCore {

namespace {

struct WipeRegistrar {
    WipeRegistrar()
    {
        TransitionFactory::instance().registerCreator(TransitionKind::Wipe, []() {
            return new WipeTransition();
        });
    }
};

static WipeRegistrar registrar;

}

}

namespace ArtifactCore {
void registerWipeTransition()
{
    TransitionFactory::instance().registerCreator(TransitionKind::Wipe, []() {
        return new WipeTransition();
    });
}
}
