module;
#include <QString>
#include <QVariant>

export module Domain.LayerTypes;

import Core.ArtifactFoundation;

export namespace ArtifactCore {

class ArtifactAbstractLayer;
class ArtifactAbstractEffect;
class ArtifactAbstractComposition;

// --- Layer (nullable shared pointer, must check before use) ---
using LayerPtr   = Ptr<ArtifactAbstractLayer>;
using LayerRef   = Ref<ArtifactAbstractLayer>;
using LayerList  = ArtifactArray<LayerPtr>;

// --- Effect ---
using EffectPtr  = Ptr<ArtifactAbstractEffect>;
using EffectRef  = Ref<ArtifactAbstractEffect>;
using EffectStack = ArtifactArray<EffectPtr>;

// --- Composition ---
using CompPtr    = Ptr<ArtifactAbstractComposition>;
using CompRef    = Ref<ArtifactAbstractComposition>;
using CompList   = ArtifactArray<CompPtr>;

// --- Property ---
using PropertyBag = ArtifactDict<QString, QVariant>;

} // namespace ArtifactCore
