module;

module Video.Transitions.BlockDissolveTransition;

import NLE.Core;
import Video.TransitionFactory;

namespace ArtifactCore {

namespace {

struct BlockDissolveRegistrar {
    BlockDissolveRegistrar() {
        TransitionFactory::instance().registerCreator(TransitionKind::BlockDissolve, []() {
            return new BlockDissolveTransition();
        });
    }
};

static BlockDissolveRegistrar registrar;

}

}
