module;

#include <QJsonObject>
#include <QString>
#include <QVector>

#include "../Define/DllExportMacro.hpp"

export module NLE.OTIO;

import NLE.Core;

export namespace ArtifactCore::NLE {

class LIBRARY_DLL_API OtioAdapter final {
public:
    static QJsonObject exportTimeline(const NLEProjectStore& store,
                                      const SequenceId& sequenceId);

    static bool importTimeline(NLEProjectStore& store,
                               const QJsonObject& timeline,
                               SequenceId* importedSequenceId = nullptr,
                               QVector<QString>* warnings = nullptr);
};

} // namespace ArtifactCore::NLE
