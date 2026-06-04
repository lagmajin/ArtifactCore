module;

#include <cstdint>
#include <algorithm>
#include <array>

export module Video.Transitions.SpinTransition;

import NLE.Core;
import Video.TransitionFactory;

namespace ArtifactCore {

namespace {

struct SpinRegistrar {
    SpinRegistrar()
    {
        TransitionFactory::instance().registerCreator(TransitionKind::Spin, []() {
            return new SpinTransition();
        });
    }
};

static SpinRegistrar registrar;

}

}