module;

#include <cstdint>
#include <compare>
#include "../Define/DllExportMacro.hpp"

#include <QString>
#include <functional>
#include <memory>
#include <QHash>

export module Video.TransitionFactory;

import NLE.Core;
import Video.AbstractTransition;

export namespace ArtifactCore {

using namespace ArtifactCore::NLE;

class LIBRARY_DLL_API TransitionFactory {
public:
    using Creator = std::function<AbstractTransition*()>;

    static TransitionFactory& instance();

    void registerCreator(TransitionKind kind, Creator creator);
    void unregister(TransitionKind kind);

    AbstractTransition* create(TransitionKind kind) const;
    QVector<TransitionKind> availableKinds() const;

private:
    QHash<TransitionKind, Creator> creators_;
};

}
