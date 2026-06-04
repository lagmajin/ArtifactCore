module;

#include <compare>
#include <utility>

module Video.TransitionFactory;

import NLE.Core;
import Video.AbstractTransition;

namespace ArtifactCore {

TransitionFactory& TransitionFactory::instance()
{
    static TransitionFactory factory;
    return factory;
}

void TransitionFactory::registerCreator(TransitionKind kind, Creator creator)
{
    creators_.insert(kind, std::move(creator));
}

void TransitionFactory::unregister(TransitionKind kind)
{
    creators_.remove(kind);
}

AbstractTransition* TransitionFactory::create(TransitionKind kind) const
{
    const auto it = creators_.constFind(kind);
    if (it == creators_.cend()) {
        return nullptr;
    }
    return it.value()();
}

QVector<TransitionKind> TransitionFactory::availableKinds() const
{
    return creators_.keys();
}

}
