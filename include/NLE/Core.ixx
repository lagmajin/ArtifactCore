module;
class tst_QList;

#include <cstdint>

#include <QColor>
#include <QHash>
#include <QHashFunctions>
#include <QJsonObject>
#include <QJsonArray>
#include <QSize>
#include <QString>
#include <QVector>

#include "../Define/DllExportMacro.hpp"

export module NLE.Core;

import Core.Defaults;
import Frame.Position;
import Frame.Range;
import Frame.Rate;

export namespace ArtifactCore::NLE {

struct LIBRARY_DLL_API TimeBase {
    int32_t numerator = 1;
    int32_t denominator = 30;
    bool dropFrame = false;

    double fps() const noexcept;
    double frameDurationSeconds() const noexcept;
    bool isValid() const noexcept;
    QJsonObject toJson() const;
    static TimeBase fromJson(const QJsonObject& json);
};

#define ARTIFACTCORE_NLE_DEFINE_ID(Name) \
struct LIBRARY_DLL_API Name { \
    quint64 value = 0; \
    constexpr bool isValid() const noexcept { return value != 0; } \
    QString toString() const; \
    static Name fromString(const QString& text); \
    bool operator==(const Name& other) const noexcept { return value == other.value; } \
    bool operator!=(const Name& other) const noexcept { return value != other.value; } \
    bool operator<(const Name& other) const noexcept { return value < other.value; } \
}; \
inline size_t qHash(const Name& id, size_t seed = 0) noexcept { return qHashMulti(seed, id.value); }

ARTIFACTCORE_NLE_DEFINE_ID(SequenceId)
ARTIFACTCORE_NLE_DEFINE_ID(TrackId)
ARTIFACTCORE_NLE_DEFINE_ID(ClipId)
ARTIFACTCORE_NLE_DEFINE_ID(MarkerId)
ARTIFACTCORE_NLE_DEFINE_ID(TransitionId)
ARTIFACTCORE_NLE_DEFINE_ID(SourceId)

#undef ARTIFACTCORE_NLE_DEFINE_ID

enum class TrackKind : quint8 {
    Video = 0,
    Audio,
    NestedSequence,
    Adjustment,
    Subtitle,
    Data
};

enum class TransitionKind : quint8 {
    Cut = 0,
    Crossfade,
    Dissolve,
    Wipe
};

enum class TrimMode : quint8 {
    Source = 0,
    Ripple,
    Roll,
    Slip,
    Slide
};

struct LIBRARY_DLL_API SourceRef {
    SourceId sourceId;
    QString uri;
    QString displayName;
    QString checksum;
    QSize frameSize;
    TimeBase timeBase;
    FrameRange availableRange = FrameRange::infinite();
    QString mimeType;
    bool online = true;
    bool proxyAvailable = false;
    bool useProxy = false;
    QString proxyUri;
    QString proxyDisplayName;
};

struct LIBRARY_DLL_API ClipDraft {
    SourceId sourceId;
    FrameRange sourceRange;
    FrameRange timelineRange;
    FrameRange trimRange;
    QString name;
    double speed = 1.0;
    double opacity = 1.0;
    bool enabled = true;
    bool locked = false;
    bool reversed = false;
    quint64 linkedGroupId = 0;
};

struct LIBRARY_DLL_API Marker {
    MarkerId id;
    SequenceId sequenceId;
    FramePosition position;
    QString name;
    QString note;
    QColor color = QColor(Qt::yellow);
};

struct LIBRARY_DLL_API Transition {
    TransitionId id;
    TrackId trackId;
    ClipId leftClipId;
    ClipId rightClipId;
    FrameRange range;
    TransitionKind kind = TransitionKind::Crossfade;
    double duration = 12.0;
    bool enabled = true;
};

struct LIBRARY_DLL_API ClipLinkGroup {
    quint64 id = 0;
    QVector<ClipId> members;
    bool videoAudioLinked = true;
    bool moveLinked = true;
    bool selectionLinked = true;
    bool trimLinked = true;
};

struct LIBRARY_DLL_API Clip {
    ClipId id;
    TrackId trackId;
    SourceId sourceId;
    FrameRange sourceRange;
    FrameRange timelineRange;
    FrameRange trimRange;
    double speed = 1.0;
    double opacity = 1.0;
    bool enabled = true;
    bool locked = false;
    bool reversed = false;
    bool selected = false;
    quint64 linkedGroupId = 0;
    QVector<TransitionId> attachedTransitions;
    QVector<MarkerId> markers;
    QString name;
};

struct LIBRARY_DLL_API Track {
    TrackId id;
    SequenceId ownerSequenceId;
    TrackKind kind = TrackKind::Video;
    QString name;
    int32_t order = 0;
    bool enabled = true;
    bool locked = false;
    bool solo = false;
    bool mute = false;
    QVector<ClipId> clipOrder;
    QVector<TransitionId> transitions;
};

struct LIBRARY_DLL_API Sequence {
    SequenceId id;
    QString name;
    TimeBase timeBase;
    FrameRange duration = FrameRange::zero();
    QVector<TrackId> trackOrder;
    QVector<MarkerId> markers;
    QString defaultLayoutName;
    bool enabled = true;
    bool locked = false;
};

struct LIBRARY_DLL_API SequenceSummary {
    SequenceId id;
    QString name;
    int trackCount = 0;
    int clipCount = 0;
    FrameRange duration = FrameRange::zero();
};

struct LIBRARY_DLL_API EditResult {
    bool success = false;
    QString message;
    QVector<SequenceId> touchedSequences;
    QVector<TrackId> touchedTracks;
    QVector<ClipId> touchedClips;
    QVector<QString> warnings;
};

struct LIBRARY_DLL_API ClipResolution {
    ClipId clipId;
    SourceRef source;
    FrameRange sourceRange;
    FrameRange effectiveRange;
    bool useProxy = false;
    bool online = false;
    QString diagnostic;
};

struct LIBRARY_DLL_API ConformReport {
    bool success = false;
    QVector<SequenceId> updatedSequences;
    QVector<ClipId> updatedClips;
    QVector<ClipId> unresolvedClips;
    QVector<QString> warnings;
};

struct LIBRARY_DLL_API ProjectStats {
    int sequenceCount = 0;
    int trackCount = 0;
    int clipCount = 0;
    int markerCount = 0;
    int transitionCount = 0;
    QVector<SequenceSummary> heavySequences;
};

struct LIBRARY_DLL_API NLEValidationIssue {
    QString code;
    QString subject;
    QString message;
};

struct LIBRARY_DLL_API NLEValidationReport {
    bool success = false;
    QVector<NLEValidationIssue> issues;
    QVector<QString> warnings;
    int brokenLinkGroupCount = 0;
    int orphanTransitionCount = 0;
    int overlappingClipCount = 0;
    int invalidClipCount = 0;
};

class LIBRARY_DLL_API NLEProjectStore {
public:
    NLEProjectStore();
    ~NLEProjectStore();

    NLEProjectStore(const NLEProjectStore&) = delete;
    NLEProjectStore& operator=(const NLEProjectStore&) = delete;
    NLEProjectStore(NLEProjectStore&&) noexcept;
    NLEProjectStore& operator=(NLEProjectStore&&) noexcept;

    SequenceId createSequence(const QString& name = QString(),
                              const TimeBase& timeBase = TimeBase{},
                              const QString& defaultLayoutName = QString());
    TrackId createTrack(const SequenceId& sequenceId,
                        TrackKind kind = TrackKind::Video,
                        const QString& name = QString());
    MarkerId createMarker(const SequenceId& sequenceId,
                          const FramePosition& position,
                          const QString& name = QString(),
                          const QString& note = QString(),
                          const QColor& color = QColor(Qt::yellow));
    TransitionId createTransition(const TrackId& trackId,
                                  const ClipId& leftClipId,
                                  const ClipId& rightClipId,
                                  const FrameRange& range,
                                  TransitionKind kind = TransitionKind::Crossfade,
                                  double duration = 12.0);

    SourceId registerSource(const SourceRef& source);
    bool relinkSource(const SourceId& sourceId, const QString& newUri,
                      const QString& displayName = QString());
    bool setSourceProxy(const SourceId& sourceId, const QString& proxyUri,
                        const QString& proxyDisplayName = QString());
    bool setSourceAvailability(const SourceId& sourceId, bool online);

    ClipId addClip(const SequenceId& sequenceId,
                   const TrackId& trackId,
                   const ClipDraft& draft);
    ClipId overwriteClip(const SequenceId& sequenceId,
                         const TrackId& trackId,
                         const ClipDraft& draft);
    bool removeClip(const ClipId& clipId);
    bool moveClip(const ClipId& clipId, const FramePosition& newTimelineStart);
    bool trimClip(const ClipId& clipId, const FrameRange& newSourceRange,
                  TrimMode mode = TrimMode::Source);
    bool rippleDelete(const ClipId& clipId);
    bool rollTrim(const ClipId& leftClipId,
                  const ClipId& rightClipId,
                  const FramePosition& boundary);
    bool slipClip(const ClipId& clipId, const FramePosition& newSourceStart);
    bool slideClip(const ClipId& clipId, const FramePosition& newTimelineStart);

    bool removeTrack(const TrackId& trackId);
    bool removeSequence(const SequenceId& sequenceId);

    Sequence* sequence(const SequenceId& sequenceId);
    const Sequence* sequence(const SequenceId& sequenceId) const;
    Track* track(const TrackId& trackId);
    const Track* track(const TrackId& trackId) const;
    Clip* clip(const ClipId& clipId);
    const Clip* clip(const ClipId& clipId) const;
    Marker* marker(const MarkerId& markerId);
    const Marker* marker(const MarkerId& markerId) const;
    Transition* transition(const TransitionId& transitionId);
    const Transition* transition(const TransitionId& transitionId) const;
    SourceRef* source(const SourceId& sourceId);
    const SourceRef* source(const SourceId& sourceId) const;

    bool hasSequence(const SequenceId& sequenceId) const;
    bool hasTrack(const TrackId& trackId) const;
    bool hasClip(const ClipId& clipId) const;
    bool hasSource(const SourceId& sourceId) const;

    QVector<SequenceId> sequenceIds() const;
    QVector<TrackId> trackIds(const SequenceId& sequenceId) const;
    QVector<ClipId> clipIds(const TrackId& trackId) const;
    QVector<ClipId> clipIdsInSequence(const SequenceId& sequenceId) const;
    QVector<SequenceSummary> sequenceSummaries(int heavyClipThreshold = 10) const;
    ProjectStats stats(int heavyClipThreshold = 10) const;
    NLEValidationReport validate() const;
    void clear();
    QJsonObject toJson() const;
    bool loadFromJson(const QJsonObject& json);

    quint64 createLinkGroup(bool videoAudioLinked = true,
                            bool selectionLinked = true,
                            bool trimLinked = true);
    bool addClipToLinkGroup(const ClipId& clipId, quint64 groupId);
    bool removeClipFromLinkGroup(const ClipId& clipId);
    bool setLinkGroupFlags(quint64 groupId,
                           bool videoAudioLinked,
                           bool moveLinked,
                           bool selectionLinked,
                           bool trimLinked);
    ClipLinkGroup* linkGroup(quint64 groupId);
    const ClipLinkGroup* linkGroup(quint64 groupId) const;
    QVector<ClipLinkGroup> linkGroups() const;

private:
    class Impl;
    Impl* impl_;
};

class LIBRARY_DLL_API SequenceEditor {
public:
    explicit SequenceEditor(NLEProjectStore* store = nullptr);
    ~SequenceEditor();

    void setStore(NLEProjectStore* store);
    NLEProjectStore* store();
    const NLEProjectStore* store() const;

    EditResult insertClip(const SequenceId& sequenceId,
                          const TrackId& trackId,
                          const ClipDraft& draft);
    EditResult overwriteClip(const SequenceId& sequenceId,
                             const TrackId& trackId,
                             const ClipDraft& draft);
    EditResult trimClip(const ClipId& clipId,
                        const FrameRange& newSourceRange,
                        TrimMode mode = TrimMode::Source);
    EditResult rippleDelete(const ClipId& clipId);
    EditResult rollTrim(const ClipId& leftClipId,
                        const ClipId& rightClipId,
                        const FramePosition& boundary);
    EditResult slipClip(const ClipId& clipId, const FramePosition& newSourceStart);
    EditResult slideClip(const ClipId& clipId, const FramePosition& newTimelineStart);
    EditResult selectClip(const ClipId& clipId, bool selected = true);

private:
    NLEProjectStore* store_ = nullptr;
};

class LIBRARY_DLL_API ClipResolver {
public:
    explicit ClipResolver(const NLEProjectStore* store = nullptr);

    void setStore(const NLEProjectStore* store);
    const NLEProjectStore* store() const;

    ClipResolution resolveClip(const ClipId& clipId) const;

private:
    const NLEProjectStore* store_ = nullptr;
};

class LIBRARY_DLL_API ConformService {
public:
    explicit ConformService(const NLEProjectStore* store = nullptr);

    void setStore(const NLEProjectStore* store);
    const NLEProjectStore* store() const;

    ConformReport conformSequence(const SequenceId& sequenceId) const;
    ConformReport conformAll() const;

private:
    const NLEProjectStore* store_ = nullptr;
};

class LIBRARY_DLL_API LinkingService {
public:
    struct LinkPropagationResult {
        bool success = false;
        QVector<ClipId> touchedClips;
        QVector<QString> warnings;
    };

    explicit LinkingService(NLEProjectStore* store = nullptr);

    void setStore(NLEProjectStore* store);
    NLEProjectStore* store();
    const NLEProjectStore* store() const;

    quint64 createLinkGroup(bool videoAudioLinked = true,
                            bool selectionLinked = true,
                            bool trimLinked = true);
    bool addClipToLinkGroup(const ClipId& clipId, quint64 groupId);
    bool removeClipFromLinkGroup(const ClipId& clipId);
    bool setLinkGroupFlags(quint64 groupId,
                           bool videoAudioLinked,
                           bool moveLinked,
                           bool selectionLinked,
                           bool trimLinked);
    QVector<ClipId> linkedClips(const ClipId& clipId) const;
    bool propagateSelectionLink(const ClipId& sourceClipId);
    bool propagateTrimLink(const ClipId& sourceClipId);
    bool propagateMoveLink(const ClipId& sourceClipId);
    LinkPropagationResult propagateSelectionLink(const ClipId& sourceClipId, bool selected);
    LinkPropagationResult propagateTrimLink(const ClipId& sourceClipId,
                                            const FrameRange& newSourceRange,
                                            TrimMode mode = TrimMode::Source);
    LinkPropagationResult propagateMoveLink(const ClipId& sourceClipId,
                                            const FramePosition& newTimelineStart);

private:
    NLEProjectStore* store_ = nullptr;
};

} // namespace ArtifactCore::NLE
