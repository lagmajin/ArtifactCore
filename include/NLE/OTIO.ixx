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
    static bool exportTimelineFile(const NLEProjectStore& store,
                                   const SequenceId& sequenceId,
                                   const QString& filePath,
                                   QVector<QString>* warnings = nullptr);

    static bool importTimeline(NLEProjectStore& store,
                               const QJsonObject& timeline,
                               SequenceId* importedSequenceId = nullptr,
                               QVector<QString>* warnings = nullptr);
    static bool importTimelineFile(NLEProjectStore& store,
                                   const QString& filePath,
                                   SequenceId* importedSequenceId = nullptr,
                                   QVector<QString>* warnings = nullptr);

    static QVector<SubtitleCue> importSrt(const QString& text,
                                          const TimeBase& timeBase = TimeBase{},
                                          QVector<QString>* warnings = nullptr);
    static QVector<SubtitleCue> importWebVtt(const QString& text,
                                              const TimeBase& timeBase = TimeBase{},
                                              QVector<QString>* warnings = nullptr);
    static QString exportSrt(const QVector<SubtitleCue>& cues,
                             const TimeBase& timeBase = TimeBase{});
    static QString exportWebVtt(const QVector<SubtitleCue>& cues,
                                const TimeBase& timeBase = TimeBase{});
    static bool importSrtIntoSequence(NLEProjectStore& store,
                                      const SequenceId& sequenceId,
                                      const QString& text,
                                      QVector<QString>* warnings = nullptr);
    static QString exportSrtFromSequence(const NLEProjectStore& store,
                                         const SequenceId& sequenceId);
    static bool importSrtFileIntoSequence(NLEProjectStore& store,
                                           const SequenceId& sequenceId,
                                           const QString& filePath,
                                           QVector<QString>* warnings = nullptr);
    static bool exportSrtFile(const NLEProjectStore& store,
                              const SequenceId& sequenceId,
                              const QString& filePath);
};

} // namespace ArtifactCore::NLE
