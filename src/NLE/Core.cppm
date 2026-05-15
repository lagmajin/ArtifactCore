module;
class tst_QList;

#include <algorithm>
#include <limits>
#include <utility>

#include <QColor>
#include <QHash>
#include <QHashFunctions>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QSize>
#include <QString>
#include <QVector>

module NLE.Core;

namespace ArtifactCore::NLE {

namespace {

template<typename TId>
QString idToString(const TId& id)
{
    return QString::number(id.value);
}

template<typename TId>
TId idFromString(const QString& text)
{
    bool ok = false;
    const quint64 value = text.toULongLong(&ok);
    if (!ok) {
        return {};
    }
    return TId{value};
}

const Clip* findClip(const QHash<ClipId, Clip>& clips, const ClipId& id)
{
    const auto it = clips.find(id);
    return it == clips.end() ? nullptr : &it.value();
}

Clip* findClip(QHash<ClipId, Clip>& clips, const ClipId& id)
{
    auto it = clips.find(id);
    return it == clips.end() ? nullptr : &it.value();
}

const Track* findTrack(const QHash<TrackId, Track>& tracks, const TrackId& id)
{
    const auto it = tracks.find(id);
    return it == tracks.end() ? nullptr : &it.value();
}

Track* findTrack(QHash<TrackId, Track>& tracks, const TrackId& id)
{
    auto it = tracks.find(id);
    return it == tracks.end() ? nullptr : &it.value();
}

const Sequence* findSequence(const QHash<SequenceId, Sequence>& sequences, const SequenceId& id)
{
    const auto it = sequences.find(id);
    return it == sequences.end() ? nullptr : &it.value();
}

Sequence* findSequence(QHash<SequenceId, Sequence>& sequences, const SequenceId& id)
{
    auto it = sequences.find(id);
    return it == sequences.end() ? nullptr : &it.value();
}

const SourceRef* findSource(const QHash<SourceId, SourceRef>& sources, const SourceId& id)
{
    const auto it = sources.find(id);
    return it == sources.end() ? nullptr : &it.value();
}

SourceRef* findSource(QHash<SourceId, SourceRef>& sources, const SourceId& id)
{
    auto it = sources.find(id);
    return it == sources.end() ? nullptr : &it.value();
}

void sortTrackClips(Track& track, const QHash<ClipId, Clip>& clips)
{
    std::sort(track.clipOrder.begin(), track.clipOrder.end(), [&clips](const ClipId& a, const ClipId& b) {
        const auto clipA = clips.find(a);
        const auto clipB = clips.find(b);
        const int64_t startA = clipA == clips.end() ? 0 : clipA.value().timelineRange.start();
        const int64_t startB = clipB == clips.end() ? 0 : clipB.value().timelineRange.start();
        if (startA != startB) {
            return startA < startB;
        }
        return a.value < b.value;
    });
}

int64_t clipDuration(const FrameRange& range)
{
    return range.isValid() ? range.duration() : 0;
}

void addIssue(NLEValidationReport& report,
              const QString& code,
              const QString& subject,
              const QString& message)
{
    report.issues.push_back(NLEValidationIssue{code, subject, message});
}

template<typename T>
void appendUnique(QVector<T>& values, const T& value)
{
    if (!values.contains(value)) {
        values.push_back(value);
    }
}

quint64 jsonValueToUInt64(const QJsonValue& value, quint64 fallback = 0)
{
    bool ok = false;
    const quint64 fromVariant = value.toVariant().toULongLong(&ok);
    if (ok) {
        return fromVariant;
    }
    bool stringOk = false;
    const quint64 fromString = value.toString().toULongLong(&stringOk);
    if (stringOk) {
        return fromString;
    }
    return fallback;
}

FrameRange inferClipRange(const ClipDraft& draft, int64_t fallbackStart)
{
    if (draft.timelineRange.isValid()) {
        return draft.timelineRange;
    }
    const int64_t duration = clipDuration(draft.sourceRange);
    if (duration > 0) {
        return FrameRange::fromDuration(fallbackStart, duration);
    }
    if (draft.trimRange.isValid()) {
        return FrameRange::fromDuration(fallbackStart, draft.trimRange.duration());
    }
    return FrameRange::fromDuration(fallbackStart, 0);
}

FrameRange remapRangeByDelta(const FrameRange& original, int64_t startDelta, int64_t endDelta)
{
    const int64_t newStart = original.start() + startDelta;
    const int64_t newEnd = original.end() + endDelta;
    if (newEnd < newStart) {
        return FrameRange::invalid();
    }
    return FrameRange(newStart, newEnd);
}

QJsonObject sourceRefToJson(const SourceRef& source)
{
    return QJsonObject{
        {QStringLiteral("sourceId"), QString::number(source.sourceId.value)},
        {QStringLiteral("uri"), source.uri},
        {QStringLiteral("displayName"), source.displayName},
        {QStringLiteral("checksum"), source.checksum},
        {QStringLiteral("frameSize"), QJsonObject{
            {QStringLiteral("width"), source.frameSize.width()},
            {QStringLiteral("height"), source.frameSize.height()}
        }},
        {QStringLiteral("timeBase"), source.timeBase.toJson()},
        {QStringLiteral("availableRange"), source.availableRange.toJson()},
        {QStringLiteral("mimeType"), source.mimeType},
        {QStringLiteral("online"), source.online},
        {QStringLiteral("proxyAvailable"), source.proxyAvailable},
        {QStringLiteral("useProxy"), source.useProxy},
        {QStringLiteral("proxyUri"), source.proxyUri},
        {QStringLiteral("proxyDisplayName"), source.proxyDisplayName}
    };
}

SourceRef sourceRefFromJson(const QJsonObject& obj)
{
    SourceRef source;
    source.sourceId = SourceId::fromString(obj.value(QStringLiteral("sourceId")).toString());
    source.uri = obj.value(QStringLiteral("uri")).toString();
    source.displayName = obj.value(QStringLiteral("displayName")).toString();
    source.checksum = obj.value(QStringLiteral("checksum")).toString();
    const QJsonObject frameSize = obj.value(QStringLiteral("frameSize")).toObject();
    source.frameSize = QSize(frameSize.value(QStringLiteral("width")).toInt(), frameSize.value(QStringLiteral("height")).toInt());
    source.timeBase = TimeBase::fromJson(obj.value(QStringLiteral("timeBase")).toObject());
    source.availableRange = FrameRange::fromJson(obj.value(QStringLiteral("availableRange")).toObject());
    source.mimeType = obj.value(QStringLiteral("mimeType")).toString();
    source.online = obj.value(QStringLiteral("online")).toBool(true);
    source.proxyAvailable = obj.value(QStringLiteral("proxyAvailable")).toBool(false);
    source.useProxy = obj.value(QStringLiteral("useProxy")).toBool(false);
    source.proxyUri = obj.value(QStringLiteral("proxyUri")).toString();
    source.proxyDisplayName = obj.value(QStringLiteral("proxyDisplayName")).toString();
    return source;
}

QJsonObject clipDraftToJson(const ClipDraft& draft)
{
    return QJsonObject{
        {QStringLiteral("sourceId"), QString::number(draft.sourceId.value)},
        {QStringLiteral("sourceRange"), draft.sourceRange.toJson()},
        {QStringLiteral("timelineRange"), draft.timelineRange.toJson()},
        {QStringLiteral("trimRange"), draft.trimRange.toJson()},
        {QStringLiteral("name"), draft.name},
        {QStringLiteral("speed"), draft.speed},
        {QStringLiteral("opacity"), draft.opacity},
        {QStringLiteral("enabled"), draft.enabled},
        {QStringLiteral("locked"), draft.locked},
        {QStringLiteral("reversed"), draft.reversed},
        {QStringLiteral("linkedGroupId"), QString::number(draft.linkedGroupId)}
    };
}

ClipDraft clipDraftFromJson(const QJsonObject& obj)
{
    ClipDraft draft;
    draft.sourceId = SourceId::fromString(obj.value(QStringLiteral("sourceId")).toString());
    draft.sourceRange = FrameRange::fromJson(obj.value(QStringLiteral("sourceRange")).toObject());
    draft.timelineRange = FrameRange::fromJson(obj.value(QStringLiteral("timelineRange")).toObject());
    draft.trimRange = FrameRange::fromJson(obj.value(QStringLiteral("trimRange")).toObject());
    draft.name = obj.value(QStringLiteral("name")).toString();
    draft.speed = obj.value(QStringLiteral("speed")).toDouble(1.0);
    draft.opacity = obj.value(QStringLiteral("opacity")).toDouble(1.0);
    draft.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
    draft.locked = obj.value(QStringLiteral("locked")).toBool(false);
    draft.reversed = obj.value(QStringLiteral("reversed")).toBool(false);
    draft.linkedGroupId = jsonValueToUInt64(obj.value(QStringLiteral("linkedGroupId")));
    return draft;
}

QJsonObject clipToJson(const Clip& clip)
{
    QJsonArray transitions;
    for (const TransitionId& id : clip.attachedTransitions) {
        transitions.append(QString::number(id.value));
    }
    QJsonArray markers;
    for (const MarkerId& id : clip.markers) {
        markers.append(QString::number(id.value));
    }
    return QJsonObject{
        {QStringLiteral("id"), QString::number(clip.id.value)},
        {QStringLiteral("trackId"), QString::number(clip.trackId.value)},
        {QStringLiteral("sourceId"), QString::number(clip.sourceId.value)},
        {QStringLiteral("sourceRange"), clip.sourceRange.toJson()},
        {QStringLiteral("timelineRange"), clip.timelineRange.toJson()},
        {QStringLiteral("trimRange"), clip.trimRange.toJson()},
        {QStringLiteral("speed"), clip.speed},
        {QStringLiteral("opacity"), clip.opacity},
        {QStringLiteral("enabled"), clip.enabled},
        {QStringLiteral("locked"), clip.locked},
        {QStringLiteral("reversed"), clip.reversed},
        {QStringLiteral("linkedGroupId"), QString::number(clip.linkedGroupId)},
        {QStringLiteral("attachedTransitions"), transitions},
        {QStringLiteral("markers"), markers},
        {QStringLiteral("name"), clip.name}
    };
}

Clip clipFromJson(const QJsonObject& obj)
{
    Clip clip;
    clip.id = ClipId::fromString(obj.value(QStringLiteral("id")).toString());
    clip.trackId = TrackId::fromString(obj.value(QStringLiteral("trackId")).toString());
    clip.sourceId = SourceId::fromString(obj.value(QStringLiteral("sourceId")).toString());
    clip.sourceRange = FrameRange::fromJson(obj.value(QStringLiteral("sourceRange")).toObject());
    clip.timelineRange = FrameRange::fromJson(obj.value(QStringLiteral("timelineRange")).toObject());
    clip.trimRange = FrameRange::fromJson(obj.value(QStringLiteral("trimRange")).toObject());
    clip.speed = obj.value(QStringLiteral("speed")).toDouble(1.0);
    clip.opacity = obj.value(QStringLiteral("opacity")).toDouble(1.0);
    clip.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
    clip.locked = obj.value(QStringLiteral("locked")).toBool(false);
    clip.reversed = obj.value(QStringLiteral("reversed")).toBool(false);
    clip.linkedGroupId = jsonValueToUInt64(obj.value(QStringLiteral("linkedGroupId")));
    clip.name = obj.value(QStringLiteral("name")).toString();
    for (const QJsonValue& value : obj.value(QStringLiteral("attachedTransitions")).toArray()) {
        clip.attachedTransitions.push_back(TransitionId::fromString(value.toString()));
    }
    for (const QJsonValue& value : obj.value(QStringLiteral("markers")).toArray()) {
        clip.markers.push_back(MarkerId::fromString(value.toString()));
    }
    return clip;
}

QJsonObject trackToJson(const Track& track)
{
    QJsonArray clips;
    for (const ClipId& id : track.clipOrder) {
        clips.append(QString::number(id.value));
    }
    QJsonArray transitions;
    for (const TransitionId& id : track.transitions) {
        transitions.append(QString::number(id.value));
    }
    return QJsonObject{
        {QStringLiteral("id"), QString::number(track.id.value)},
        {QStringLiteral("ownerSequenceId"), QString::number(track.ownerSequenceId.value)},
        {QStringLiteral("kind"), static_cast<int>(track.kind)},
        {QStringLiteral("name"), track.name},
        {QStringLiteral("order"), track.order},
        {QStringLiteral("enabled"), track.enabled},
        {QStringLiteral("locked"), track.locked},
        {QStringLiteral("solo"), track.solo},
        {QStringLiteral("mute"), track.mute},
        {QStringLiteral("clipOrder"), clips},
        {QStringLiteral("transitions"), transitions}
    };
}

Track trackFromJson(const QJsonObject& obj)
{
    Track track;
    track.id = TrackId::fromString(obj.value(QStringLiteral("id")).toString());
    track.ownerSequenceId = SequenceId::fromString(obj.value(QStringLiteral("ownerSequenceId")).toString());
    track.kind = static_cast<TrackKind>(obj.value(QStringLiteral("kind")).toInt(static_cast<int>(TrackKind::Video)));
    track.name = obj.value(QStringLiteral("name")).toString();
    track.order = obj.value(QStringLiteral("order")).toInt();
    track.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
    track.locked = obj.value(QStringLiteral("locked")).toBool(false);
    track.solo = obj.value(QStringLiteral("solo")).toBool(false);
    track.mute = obj.value(QStringLiteral("mute")).toBool(false);
    for (const QJsonValue& value : obj.value(QStringLiteral("clipOrder")).toArray()) {
        track.clipOrder.push_back(ClipId::fromString(value.toString()));
    }
    for (const QJsonValue& value : obj.value(QStringLiteral("transitions")).toArray()) {
        track.transitions.push_back(TransitionId::fromString(value.toString()));
    }
    return track;
}

QJsonObject sequenceToJson(const Sequence& sequence)
{
    QJsonArray tracks;
    for (const TrackId& id : sequence.trackOrder) {
        tracks.append(QString::number(id.value));
    }
    QJsonArray markers;
    for (const MarkerId& id : sequence.markers) {
        markers.append(QString::number(id.value));
    }
    return QJsonObject{
        {QStringLiteral("id"), QString::number(sequence.id.value)},
        {QStringLiteral("name"), sequence.name},
        {QStringLiteral("timeBase"), sequence.timeBase.toJson()},
        {QStringLiteral("duration"), sequence.duration.toJson()},
        {QStringLiteral("trackOrder"), tracks},
        {QStringLiteral("markers"), markers},
        {QStringLiteral("defaultLayoutName"), sequence.defaultLayoutName},
        {QStringLiteral("enabled"), sequence.enabled},
        {QStringLiteral("locked"), sequence.locked}
    };
}

Sequence sequenceFromJson(const QJsonObject& obj)
{
    Sequence sequence;
    sequence.id = SequenceId::fromString(obj.value(QStringLiteral("id")).toString());
    sequence.name = obj.value(QStringLiteral("name")).toString();
    sequence.timeBase = TimeBase::fromJson(obj.value(QStringLiteral("timeBase")).toObject());
    sequence.duration = FrameRange::fromJson(obj.value(QStringLiteral("duration")).toObject());
    sequence.defaultLayoutName = obj.value(QStringLiteral("defaultLayoutName")).toString();
    sequence.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
    sequence.locked = obj.value(QStringLiteral("locked")).toBool(false);
    for (const QJsonValue& value : obj.value(QStringLiteral("trackOrder")).toArray()) {
        sequence.trackOrder.push_back(TrackId::fromString(value.toString()));
    }
    for (const QJsonValue& value : obj.value(QStringLiteral("markers")).toArray()) {
        sequence.markers.push_back(MarkerId::fromString(value.toString()));
    }
    return sequence;
}

QJsonObject markerToJson(const Marker& marker)
{
    return QJsonObject{
        {QStringLiteral("id"), QString::number(marker.id.value)},
        {QStringLiteral("sequenceId"), QString::number(marker.sequenceId.value)},
        {QStringLiteral("position"), marker.position.framePosition()},
        {QStringLiteral("name"), marker.name},
        {QStringLiteral("note"), marker.note},
        {QStringLiteral("color"), marker.color.name(QColor::HexArgb)}
    };
}

Marker markerFromJson(const QJsonObject& obj)
{
    Marker marker;
    marker.id = MarkerId::fromString(obj.value(QStringLiteral("id")).toString());
    marker.sequenceId = SequenceId::fromString(obj.value(QStringLiteral("sequenceId")).toString());
    marker.position = FramePosition(obj.value(QStringLiteral("position")).toInt());
    marker.name = obj.value(QStringLiteral("name")).toString();
    marker.note = obj.value(QStringLiteral("note")).toString();
    marker.color = QColor(obj.value(QStringLiteral("color")).toString());
    return marker;
}

QJsonObject transitionToJson(const Transition& transition)
{
    return QJsonObject{
        {QStringLiteral("id"), QString::number(transition.id.value)},
        {QStringLiteral("trackId"), QString::number(transition.trackId.value)},
        {QStringLiteral("leftClipId"), QString::number(transition.leftClipId.value)},
        {QStringLiteral("rightClipId"), QString::number(transition.rightClipId.value)},
        {QStringLiteral("range"), transition.range.toJson()},
        {QStringLiteral("kind"), static_cast<int>(transition.kind)},
        {QStringLiteral("duration"), transition.duration},
        {QStringLiteral("enabled"), transition.enabled}
    };
}

Transition transitionFromJson(const QJsonObject& obj)
{
    Transition transition;
    transition.id = TransitionId::fromString(obj.value(QStringLiteral("id")).toString());
    transition.trackId = TrackId::fromString(obj.value(QStringLiteral("trackId")).toString());
    transition.leftClipId = ClipId::fromString(obj.value(QStringLiteral("leftClipId")).toString());
    transition.rightClipId = ClipId::fromString(obj.value(QStringLiteral("rightClipId")).toString());
    transition.range = FrameRange::fromJson(obj.value(QStringLiteral("range")).toObject());
    transition.kind = static_cast<TransitionKind>(obj.value(QStringLiteral("kind")).toInt(static_cast<int>(TransitionKind::Crossfade)));
    transition.duration = obj.value(QStringLiteral("duration")).toDouble(12.0);
    transition.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
    return transition;
}

QJsonObject linkGroupToJson(const ClipLinkGroup& group)
{
    QJsonArray members;
    for (const ClipId& id : group.members) {
        members.append(QString::number(id.value));
    }
    return QJsonObject{
        {QStringLiteral("id"), QString::number(group.id)},
        {QStringLiteral("members"), members},
        {QStringLiteral("videoAudioLinked"), group.videoAudioLinked},
        {QStringLiteral("moveLinked"), group.moveLinked},
        {QStringLiteral("selectionLinked"), group.selectionLinked},
        {QStringLiteral("trimLinked"), group.trimLinked}
    };
}

ClipLinkGroup linkGroupFromJson(const QJsonObject& obj)
{
    ClipLinkGroup group;
    group.id = jsonValueToUInt64(obj.value(QStringLiteral("id")));
    group.videoAudioLinked = obj.value(QStringLiteral("videoAudioLinked")).toBool(true);
    group.moveLinked = obj.value(QStringLiteral("moveLinked")).toBool(true);
    group.selectionLinked = obj.value(QStringLiteral("selectionLinked")).toBool(true);
    group.trimLinked = obj.value(QStringLiteral("trimLinked")).toBool(true);
    for (const QJsonValue& value : obj.value(QStringLiteral("members")).toArray()) {
        group.members.push_back(ClipId::fromString(value.toString()));
    }
    return group;
}

} // namespace

double TimeBase::fps() const noexcept
{
    if (!isValid()) {
        return 0.0;
    }
    return static_cast<double>(denominator) / static_cast<double>(numerator);
}

double TimeBase::frameDurationSeconds() const noexcept
{
    const double currentFps = fps();
    return currentFps > 0.0 ? 1.0 / currentFps : 0.0;
}

bool TimeBase::isValid() const noexcept
{
    return numerator > 0 && denominator > 0;
}

QJsonObject TimeBase::toJson() const
{
    return QJsonObject{
        {QStringLiteral("numerator"), numerator},
        {QStringLiteral("denominator"), denominator},
        {QStringLiteral("dropFrame"), dropFrame}
    };
}

TimeBase TimeBase::fromJson(const QJsonObject& json)
{
    TimeBase timeBase;
    timeBase.numerator = json.value(QStringLiteral("numerator")).toInt(1);
    timeBase.denominator = json.value(QStringLiteral("denominator")).toInt(30);
    timeBase.dropFrame = json.value(QStringLiteral("dropFrame")).toBool(false);
    return timeBase;
}

QString SequenceId::toString() const { return idToString(*this); }
QString TrackId::toString() const { return idToString(*this); }
QString ClipId::toString() const { return idToString(*this); }
QString MarkerId::toString() const { return idToString(*this); }
QString TransitionId::toString() const { return idToString(*this); }
QString SourceId::toString() const { return idToString(*this); }

SequenceId SequenceId::fromString(const QString& text) { return idFromString<SequenceId>(text); }
TrackId TrackId::fromString(const QString& text) { return idFromString<TrackId>(text); }
ClipId ClipId::fromString(const QString& text) { return idFromString<ClipId>(text); }
MarkerId MarkerId::fromString(const QString& text) { return idFromString<MarkerId>(text); }
TransitionId TransitionId::fromString(const QString& text) { return idFromString<TransitionId>(text); }
SourceId SourceId::fromString(const QString& text) { return idFromString<SourceId>(text); }

class NLEProjectStore::Impl {
public:
    quint64 nextSequenceId = 1;
    quint64 nextTrackId = 1;
    quint64 nextClipId = 1;
    quint64 nextMarkerId = 1;
    quint64 nextTransitionId = 1;
    quint64 nextSourceId = 1;
    quint64 nextLinkGroupId = 1;

    QHash<SequenceId, Sequence> sequences;
    QHash<TrackId, Track> tracks;
    QHash<ClipId, Clip> clips;
    QHash<MarkerId, Marker> markers;
    QHash<TransitionId, Transition> transitions;
    QHash<SourceId, SourceRef> sources;
    QHash<quint64, ClipLinkGroup> linkGroups;

    QVector<ClipId> clipsOnTrack(const TrackId& trackId) const
    {
        const auto trackIt = tracks.find(trackId);
        if (trackIt == tracks.end()) {
            return {};
        }
        QVector<ClipId> ids = trackIt->clipOrder;
        std::sort(ids.begin(), ids.end(), [this](const ClipId& a, const ClipId& b) {
            const auto clipA = this->clips.find(a);
            const auto clipB = this->clips.find(b);
            const int64_t startA = clipA == this->clips.end() ? 0 : clipA.value().timelineRange.start();
            const int64_t startB = clipB == this->clips.end() ? 0 : clipB.value().timelineRange.start();
            if (startA != startB) {
                return startA < startB;
            }
            return a.value < b.value;
        });
        return ids;
    }

    QVector<ClipId> clipsInSequence(const SequenceId& sequenceId) const
    {
        QVector<ClipId> result;
        const auto seqIt = sequences.find(sequenceId);
        if (seqIt == sequences.end()) {
            return result;
        }
        for (const TrackId& trackId : seqIt->trackOrder) {
            result += clipsOnTrack(trackId);
        }
        return result;
    }

    void sortTrack(const TrackId& trackId)
    {
        auto trackIt = tracks.find(trackId);
        if (trackIt == tracks.end()) {
            return;
        }
        sortTrackClips(*trackIt, clips);
    }

    int64_t trackEndFrame(const TrackId& trackId) const
    {
        const auto trackIt = tracks.find(trackId);
        if (trackIt == tracks.end()) {
            return 0;
        }
        int64_t endFrame = 0;
        for (const ClipId& clipId : trackIt->clipOrder) {
            const auto clipIt = clips.find(clipId);
            if (clipIt == clips.end() || !clipIt->timelineRange.isValid()) {
                continue;
            }
            endFrame = std::max(endFrame, clipIt->timelineRange.end());
        }
        return endFrame;
    }

    void recomputeSequenceDuration(const SequenceId& sequenceId)
    {
        auto seqIt = sequences.find(sequenceId);
        if (seqIt == sequences.end()) {
            return;
        }

        int64_t endFrame = 0;
        for (const TrackId& trackId : seqIt->trackOrder) {
            const auto trackIt = tracks.find(trackId);
            if (trackIt == tracks.end()) {
                continue;
            }
            for (const ClipId& clipId : trackIt->clipOrder) {
                const auto clipIt = clips.find(clipId);
                if (clipIt == clips.end() || !clipIt->timelineRange.isValid()) {
                    continue;
                }
                endFrame = std::max(endFrame, clipIt->timelineRange.end());
            }
        }
        for (const MarkerId& markerId : seqIt->markers) {
            const auto markerIt = markers.find(markerId);
            if (markerIt != markers.end()) {
                endFrame = std::max(endFrame, markerIt->position.framePosition());
            }
        }
        seqIt->duration = FrameRange::fromDuration(0, endFrame);
    }

    void shiftClipsAfter(const TrackId& trackId,
                         int64_t fromFrame,
                         int64_t delta,
                         const ClipId& skipClip = ClipId{})
    {
        if (delta == 0) {
            return;
        }
        auto trackIt = tracks.find(trackId);
        if (trackIt == tracks.end()) {
            return;
        }
        for (const ClipId& clipId : trackIt->clipOrder) {
            if (clipId == skipClip) {
                continue;
            }
            auto clipIt = clips.find(clipId);
            if (clipIt == clips.end() || !clipIt->timelineRange.isValid()) {
                continue;
            }
            if (clipIt->timelineRange.start() >= fromFrame) {
                clipIt->timelineRange.shift(delta);
            }
        }
        sortTrack(trackId);
    }

    SequenceSummary summarizeSequence(const SequenceId& sequenceId) const
    {
        SequenceSummary summary;
        const auto seqIt = sequences.find(sequenceId);
        if (seqIt == sequences.end()) {
            return summary;
        }
        summary.id = seqIt->id;
        summary.name = seqIt->name;
        summary.trackCount = seqIt->trackOrder.size();
        summary.duration = seqIt->duration;
        for (const TrackId& trackId : seqIt->trackOrder) {
            const auto trackIt = tracks.find(trackId);
            if (trackIt != tracks.end()) {
                summary.clipCount += trackIt->clipOrder.size();
            }
        }
        return summary;
    }

    void clear()
    {
        nextSequenceId = 1;
        nextTrackId = 1;
        nextClipId = 1;
        nextMarkerId = 1;
        nextTransitionId = 1;
        nextSourceId = 1;
        nextLinkGroupId = 1;
        sequences.clear();
        tracks.clear();
        clips.clear();
        markers.clear();
        transitions.clear();
        sources.clear();
        linkGroups.clear();
    }
};

NLEProjectStore::NLEProjectStore()
    : impl_(new Impl())
{
}

NLEProjectStore::~NLEProjectStore()
{
    delete impl_;
}

NLEProjectStore::NLEProjectStore(NLEProjectStore&& other) noexcept
    : impl_(other.impl_)
{
    other.impl_ = nullptr;
}

NLEProjectStore& NLEProjectStore::operator=(NLEProjectStore&& other) noexcept
{
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

SequenceId NLEProjectStore::createSequence(const QString& name,
                                           const TimeBase& timeBase,
                                           const QString& defaultLayoutName)
{
    Sequence sequence;
    sequence.id = SequenceId{impl_->nextSequenceId++};
    sequence.name = name.isEmpty() ? QStringLiteral("Sequence %1").arg(sequence.id.value) : name;
    sequence.timeBase = timeBase.isValid() ? timeBase : TimeBase{};
    sequence.defaultLayoutName = defaultLayoutName;
    impl_->sequences.insert(sequence.id, sequence);
    return sequence.id;
}

TrackId NLEProjectStore::createTrack(const SequenceId& sequenceId,
                                    TrackKind kind,
                                    const QString& name)
{
    auto seq = findSequence(impl_->sequences, sequenceId);
    if (!seq) {
        return {};
    }

    Track track;
    track.id = TrackId{impl_->nextTrackId++};
    track.ownerSequenceId = sequenceId;
    track.kind = kind;
    track.order = seq->trackOrder.size();
    track.name = name.isEmpty() ? QStringLiteral("Track %1").arg(track.order + 1) : name;

    seq->trackOrder.push_back(track.id);
    impl_->tracks.insert(track.id, track);
    return track.id;
}

MarkerId NLEProjectStore::createMarker(const SequenceId& sequenceId,
                                      const FramePosition& position,
                                      const QString& name,
                                      const QString& note,
                                      const QColor& color)
{
    auto seq = findSequence(impl_->sequences, sequenceId);
    if (!seq) {
        return {};
    }

    Marker marker;
    marker.id = MarkerId{impl_->nextMarkerId++};
    marker.sequenceId = sequenceId;
    marker.position = position;
    marker.name = name;
    marker.note = note;
    marker.color = color;

    impl_->markers.insert(marker.id, marker);
    seq->markers.push_back(marker.id);
    impl_->recomputeSequenceDuration(sequenceId);
    return marker.id;
}

TransitionId NLEProjectStore::createTransition(const TrackId& trackId,
                                              const ClipId& leftClipId,
                                              const ClipId& rightClipId,
                                              const FrameRange& range,
                                              TransitionKind kind,
                                              double duration)
{
    if (!impl_->tracks.contains(trackId) ||
        !impl_->clips.contains(leftClipId) ||
        !impl_->clips.contains(rightClipId)) {
        return {};
    }

    Transition transition;
    transition.id = TransitionId{impl_->nextTransitionId++};
    transition.trackId = trackId;
    transition.leftClipId = leftClipId;
    transition.rightClipId = rightClipId;
    transition.range = range;
    transition.kind = kind;
    transition.duration = duration;
    impl_->transitions.insert(transition.id, transition);

    impl_->clips[leftClipId].attachedTransitions.push_back(transition.id);
    impl_->clips[rightClipId].attachedTransitions.push_back(transition.id);
    return transition.id;
}

SourceId NLEProjectStore::registerSource(const SourceRef& source)
{
    SourceRef copy = source;
    if (!copy.sourceId.isValid()) {
        copy.sourceId = SourceId{impl_->nextSourceId++};
    }
    impl_->sources.insert(copy.sourceId, copy);
    return copy.sourceId;
}

bool NLEProjectStore::relinkSource(const SourceId& sourceId, const QString& newUri, const QString& displayName)
{
    auto source = findSource(impl_->sources, sourceId);
    if (!source) {
        return false;
    }
    source->uri = newUri;
    if (!displayName.isEmpty()) {
        source->displayName = displayName;
    }
    source->online = true;
    source->checksum.clear();
    return true;
}

bool NLEProjectStore::setSourceProxy(const SourceId& sourceId, const QString& proxyUri, const QString& proxyDisplayName)
{
    auto source = findSource(impl_->sources, sourceId);
    if (!source) {
        return false;
    }
    source->proxyUri = proxyUri;
    source->proxyDisplayName = proxyDisplayName;
    source->proxyAvailable = !proxyUri.isEmpty();
    return true;
}

bool NLEProjectStore::setSourceAvailability(const SourceId& sourceId, bool online)
{
    auto source = findSource(impl_->sources, sourceId);
    if (!source) {
        return false;
    }
    source->online = online;
    return true;
}

ClipId NLEProjectStore::addClip(const SequenceId& sequenceId,
                                const TrackId& trackId,
                                const ClipDraft& draft)
{
    auto seq = findSequence(impl_->sequences, sequenceId);
    auto track = findTrack(impl_->tracks, trackId);
    if (!seq || !track || track->ownerSequenceId != sequenceId) {
        return {};
    }

    const FrameRange durationRange = draft.timelineRange.isValid()
        ? draft.timelineRange
        : (draft.sourceRange.isValid() ? draft.sourceRange : draft.trimRange);
    const int64_t duration = clipDuration(durationRange);
    if (duration <= 0) {
        return {};
    }

    const int64_t insertAt = draft.timelineRange.isValid()
        ? draft.timelineRange.start()
        : track->clipOrder.isEmpty() ? 0 : impl_->trackEndFrame(trackId);

    impl_->shiftClipsAfter(trackId, insertAt, duration);

    Clip clip;
    clip.id = ClipId{impl_->nextClipId++};
    clip.trackId = trackId;
    clip.sourceId = draft.sourceId;
    clip.sourceRange = draft.sourceRange.isValid()
        ? draft.sourceRange
        : FrameRange::fromDuration(0, duration);
    clip.timelineRange = inferClipRange(draft, insertAt);
    clip.trimRange = draft.trimRange.isValid() ? draft.trimRange : clip.sourceRange;
    clip.speed = draft.speed;
    clip.opacity = draft.opacity;
    clip.enabled = draft.enabled;
    clip.locked = draft.locked;
    clip.reversed = draft.reversed;
    clip.linkedGroupId = draft.linkedGroupId;
    clip.name = draft.name;

    impl_->clips.insert(clip.id, clip);
    track->clipOrder.push_back(clip.id);
    impl_->sortTrack(trackId);
    impl_->recomputeSequenceDuration(sequenceId);
    return clip.id;
}

ClipId NLEProjectStore::overwriteClip(const SequenceId& sequenceId,
                                     const TrackId& trackId,
                                     const ClipDraft& draft)
{
    auto seq = findSequence(impl_->sequences, sequenceId);
    auto track = findTrack(impl_->tracks, trackId);
    if (!seq || !track || track->ownerSequenceId != sequenceId) {
        return {};
    }

    const FrameRange durationRange = draft.timelineRange.isValid()
        ? draft.timelineRange
        : (draft.sourceRange.isValid() ? draft.sourceRange : draft.trimRange);
    const int64_t duration = clipDuration(durationRange);
    if (duration <= 0) {
        return {};
    }

    const int64_t insertAt = draft.timelineRange.isValid()
        ? draft.timelineRange.start()
        : track->clipOrder.isEmpty() ? 0 : impl_->trackEndFrame(trackId);
    const FrameRange targetRange = FrameRange::fromDuration(insertAt, duration);

    QVector<ClipId> overlapped;
    for (const ClipId& clipId : track->clipOrder) {
        const auto clip = findClip(impl_->clips, clipId);
        if (clip && clip->timelineRange.overlaps(targetRange)) {
            overlapped.push_back(clipId);
        }
    }
    for (const ClipId& clipId : overlapped) {
        removeClip(clipId);
    }

    Clip clip;
    clip.id = ClipId{impl_->nextClipId++};
    clip.trackId = trackId;
    clip.sourceId = draft.sourceId;
    clip.sourceRange = draft.sourceRange.isValid()
        ? draft.sourceRange
        : FrameRange::fromDuration(0, duration);
    clip.timelineRange = targetRange;
    clip.trimRange = draft.trimRange.isValid() ? draft.trimRange : clip.sourceRange;
    clip.speed = draft.speed;
    clip.opacity = draft.opacity;
    clip.enabled = draft.enabled;
    clip.locked = draft.locked;
    clip.reversed = draft.reversed;
    clip.linkedGroupId = draft.linkedGroupId;
    clip.name = draft.name;

    impl_->clips.insert(clip.id, clip);
    track->clipOrder.push_back(clip.id);
    impl_->sortTrack(trackId);
    impl_->recomputeSequenceDuration(sequenceId);
    return clip.id;
}

bool NLEProjectStore::removeClip(const ClipId& clipId)
{
    auto clip = findClip(impl_->clips, clipId);
    if (!clip) {
        return false;
    }

    const TrackId trackId = clip->trackId;
    const SequenceId sequenceId = impl_->tracks.contains(trackId)
        ? impl_->tracks[trackId].ownerSequenceId
        : SequenceId{};

    if (auto track = findTrack(impl_->tracks, trackId)) {
        track->clipOrder.removeAll(clipId);
    }
    impl_->clips.remove(clipId);

    if (sequenceId.isValid()) {
        impl_->recomputeSequenceDuration(sequenceId);
    }
    return true;
}

bool NLEProjectStore::moveClip(const ClipId& clipId, const FramePosition& newTimelineStart)
{
    auto clip = findClip(impl_->clips, clipId);
    if (!clip || !clip->timelineRange.isValid()) {
        return false;
    }

    const int64_t delta = newTimelineStart.framePosition() - clip->timelineRange.start();
    if (delta == 0) {
        return true;
    }

    clip->timelineRange.shift(delta);
    if (auto track = findTrack(impl_->tracks, clip->trackId)) {
        impl_->sortTrack(track->id);
        impl_->recomputeSequenceDuration(track->ownerSequenceId);
    }
    return true;
}

bool NLEProjectStore::trimClip(const ClipId& clipId,
                               const FrameRange& newSourceRange,
                               TrimMode mode)
{
    auto clip = findClip(impl_->clips, clipId);
    if (!clip || !newSourceRange.isValid()) {
        return false;
    }

    const int64_t oldDuration = clip->timelineRange.duration();
    const int64_t newDuration = newSourceRange.duration();
    const int64_t delta = newDuration - oldDuration;

    switch (mode) {
    case TrimMode::Source:
        clip->sourceRange = newSourceRange;
        clip->trimRange = newSourceRange;
        clip->timelineRange.setDuration(newDuration);
        break;
    case TrimMode::Ripple:
        clip->sourceRange = newSourceRange;
        clip->trimRange = newSourceRange;
        clip->timelineRange.setDuration(newDuration);
        impl_->shiftClipsAfter(clip->trackId, clip->timelineRange.end(), delta, clipId);
        break;
    case TrimMode::Slip:
        clip->sourceRange = FrameRange::fromDuration(newSourceRange.start(), oldDuration);
        clip->trimRange = clip->sourceRange;
        break;
    case TrimMode::Slide:
        clip->timelineRange.setDuration(newDuration);
        break;
    case TrimMode::Roll:
        return false;
    }

    if (auto track = findTrack(impl_->tracks, clip->trackId)) {
        impl_->sortTrack(track->id);
        impl_->recomputeSequenceDuration(track->ownerSequenceId);
    }
    return true;
}

bool NLEProjectStore::rippleDelete(const ClipId& clipId)
{
    auto clip = findClip(impl_->clips, clipId);
    if (!clip || !clip->timelineRange.isValid()) {
        return false;
    }

    const TrackId trackId = clip->trackId;
    const SequenceId sequenceId = impl_->tracks.contains(trackId)
        ? impl_->tracks[trackId].ownerSequenceId
        : SequenceId{};
    const int64_t delta = -clip->timelineRange.duration();
    const int64_t fromFrame = clip->timelineRange.end();

    impl_->clips.remove(clipId);
    if (auto track = findTrack(impl_->tracks, trackId)) {
        track->clipOrder.removeAll(clipId);
        impl_->shiftClipsAfter(trackId, fromFrame, delta);
        impl_->sortTrack(trackId);
    }
    if (sequenceId.isValid()) {
        impl_->recomputeSequenceDuration(sequenceId);
    }
    return true;
}

bool NLEProjectStore::rollTrim(const ClipId& leftClipId,
                               const ClipId& rightClipId,
                               const FramePosition& boundary)
{
    auto left = findClip(impl_->clips, leftClipId);
    auto right = findClip(impl_->clips, rightClipId);
    if (!left || !right || left->trackId != right->trackId) {
        return false;
    }
    if (!left->timelineRange.isValid() || !right->timelineRange.isValid()) {
        return false;
    }

    const int64_t oldLeftEnd = left->timelineRange.end();
    const int64_t oldRightStart = right->timelineRange.start();
    const int64_t newBoundary = boundary.framePosition();
    const int64_t leftDelta = newBoundary - oldLeftEnd;
    const int64_t rightDelta = oldRightStart - newBoundary;
    const int64_t leftNewDuration = left->timelineRange.duration() + leftDelta;
    const int64_t rightNewDuration = right->timelineRange.duration() - leftDelta;

    if (leftNewDuration <= 0 || rightNewDuration <= 0) {
        return false;
    }

    left->timelineRange.setEnd(newBoundary);
    right->timelineRange.setStart(newBoundary);
    left->sourceRange.setEnd(left->sourceRange.start() + leftNewDuration);
    right->sourceRange.setStart(right->sourceRange.start() + rightDelta);
    right->sourceRange.setEnd(right->sourceRange.start() + rightNewDuration);
    left->trimRange = left->sourceRange;
    right->trimRange = right->sourceRange;

    if (auto track = findTrack(impl_->tracks, left->trackId)) {
        impl_->sortTrack(track->id);
        impl_->recomputeSequenceDuration(track->ownerSequenceId);
    }
    return true;
}

bool NLEProjectStore::slipClip(const ClipId& clipId, const FramePosition& newSourceStart)
{
    auto clip = findClip(impl_->clips, clipId);
    if (!clip || !clip->timelineRange.isValid()) {
        return false;
    }
    const int64_t duration = clip->timelineRange.duration();
    clip->sourceRange = FrameRange::fromDuration(newSourceStart.framePosition(), duration);
    clip->trimRange = clip->sourceRange;
    return true;
}

bool NLEProjectStore::slideClip(const ClipId& clipId, const FramePosition& newTimelineStart)
{
    return moveClip(clipId, newTimelineStart);
}

bool NLEProjectStore::removeTrack(const TrackId& trackId)
{
    auto track = findTrack(impl_->tracks, trackId);
    if (!track) {
        return false;
    }
    const SequenceId sequenceId = track->ownerSequenceId;
    const QVector<ClipId> clipOrder = track->clipOrder;
    for (const ClipId& clipId : clipOrder) {
        removeClip(clipId);
    }
    if (auto sequence = findSequence(impl_->sequences, sequenceId)) {
        sequence->trackOrder.removeAll(trackId);
    }
    impl_->tracks.remove(trackId);
    if (sequenceId.isValid()) {
        impl_->recomputeSequenceDuration(sequenceId);
    }
    return true;
}

bool NLEProjectStore::removeSequence(const SequenceId& sequenceId)
{
    auto sequence = findSequence(impl_->sequences, sequenceId);
    if (!sequence) {
        return false;
    }
    const QVector<TrackId> tracks = sequence->trackOrder;
    const QVector<MarkerId> markers = sequence->markers;
    for (const TrackId& trackId : tracks) {
        removeTrack(trackId);
    }
    for (const MarkerId& markerId : markers) {
        impl_->markers.remove(markerId);
    }
    impl_->sequences.remove(sequenceId);
    return true;
}

Sequence* NLEProjectStore::sequence(const SequenceId& sequenceId)
{
    return findSequence(impl_->sequences, sequenceId);
}

const Sequence* NLEProjectStore::sequence(const SequenceId& sequenceId) const
{
    return findSequence(impl_->sequences, sequenceId);
}

Track* NLEProjectStore::track(const TrackId& trackId)
{
    return findTrack(impl_->tracks, trackId);
}

const Track* NLEProjectStore::track(const TrackId& trackId) const
{
    return findTrack(impl_->tracks, trackId);
}

Clip* NLEProjectStore::clip(const ClipId& clipId)
{
    return findClip(impl_->clips, clipId);
}

const Clip* NLEProjectStore::clip(const ClipId& clipId) const
{
    return findClip(impl_->clips, clipId);
}

Marker* NLEProjectStore::marker(const MarkerId& markerId)
{
    const auto it = impl_->markers.find(markerId);
    return it == impl_->markers.end() ? nullptr : &it.value();
}

const Marker* NLEProjectStore::marker(const MarkerId& markerId) const
{
    const auto it = impl_->markers.find(markerId);
    return it == impl_->markers.end() ? nullptr : &it.value();
}

Transition* NLEProjectStore::transition(const TransitionId& transitionId)
{
    const auto it = impl_->transitions.find(transitionId);
    return it == impl_->transitions.end() ? nullptr : &it.value();
}

const Transition* NLEProjectStore::transition(const TransitionId& transitionId) const
{
    const auto it = impl_->transitions.find(transitionId);
    return it == impl_->transitions.end() ? nullptr : &it.value();
}

SourceRef* NLEProjectStore::source(const SourceId& sourceId)
{
    return findSource(impl_->sources, sourceId);
}

const SourceRef* NLEProjectStore::source(const SourceId& sourceId) const
{
    return findSource(impl_->sources, sourceId);
}

bool NLEProjectStore::hasSequence(const SequenceId& sequenceId) const
{
    return impl_->sequences.contains(sequenceId);
}

bool NLEProjectStore::hasTrack(const TrackId& trackId) const
{
    return impl_->tracks.contains(trackId);
}

bool NLEProjectStore::hasClip(const ClipId& clipId) const
{
    return impl_->clips.contains(clipId);
}

bool NLEProjectStore::hasSource(const SourceId& sourceId) const
{
    return impl_->sources.contains(sourceId);
}

QVector<SequenceId> NLEProjectStore::sequenceIds() const
{
    QVector<SequenceId> ids;
    ids.reserve(impl_->sequences.size());
    for (const auto& id : impl_->sequences.keys()) {
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end(), [](const SequenceId& a, const SequenceId& b) {
        return a.value < b.value;
    });
    return ids;
}

QVector<TrackId> NLEProjectStore::trackIds(const SequenceId& sequenceId) const
{
    const auto sequence = findSequence(impl_->sequences, sequenceId);
    if (!sequence) {
        return {};
    }
    QVector<TrackId> ids = sequence->trackOrder;
    std::sort(ids.begin(), ids.end(), [this](const TrackId& a, const TrackId& b) {
        const auto ta = impl_->tracks.find(a);
        const auto tb = impl_->tracks.find(b);
        const int oa = ta == impl_->tracks.end() ? 0 : ta.value().order;
        const int ob = tb == impl_->tracks.end() ? 0 : tb.value().order;
        if (oa != ob) {
            return oa < ob;
        }
        return a.value < b.value;
    });
    return ids;
}

QVector<ClipId> NLEProjectStore::clipIds(const TrackId& trackId) const
{
    return impl_->clipsOnTrack(trackId);
}

QVector<ClipId> NLEProjectStore::clipIdsInSequence(const SequenceId& sequenceId) const
{
    return impl_->clipsInSequence(sequenceId);
}

QVector<SequenceSummary> NLEProjectStore::sequenceSummaries(int heavyClipThreshold) const
{
    QVector<SequenceSummary> summaries;
    summaries.reserve(impl_->sequences.size());
    for (const SequenceId& id : sequenceIds()) {
        summaries.push_back(impl_->summarizeSequence(id));
    }
    if (heavyClipThreshold > 0) {
        std::sort(summaries.begin(), summaries.end(), [heavyClipThreshold](const SequenceSummary& a, const SequenceSummary& b) {
            const bool aHeavy = a.clipCount >= heavyClipThreshold;
            const bool bHeavy = b.clipCount >= heavyClipThreshold;
            if (aHeavy != bHeavy) {
                return aHeavy && !bHeavy;
            }
            if (a.clipCount != b.clipCount) {
                return a.clipCount > b.clipCount;
            }
            return a.id.value < b.id.value;
        });
    }
    return summaries;
}

ProjectStats NLEProjectStore::stats(int heavyClipThreshold) const
{
    ProjectStats stats;
    stats.sequenceCount = impl_->sequences.size();
    stats.trackCount = impl_->tracks.size();
    stats.clipCount = impl_->clips.size();
    stats.markerCount = impl_->markers.size();
    stats.transitionCount = impl_->transitions.size();

    for (const SequenceSummary& summary : sequenceSummaries(heavyClipThreshold)) {
        if (summary.clipCount >= heavyClipThreshold) {
            stats.heavySequences.push_back(summary);
        }
    }
    return stats;
}

NLEValidationReport NLEProjectStore::validate() const
{
    NLEValidationReport report;
    report.success = true;

    for (const ClipId& clipId : impl_->clips.keys()) {
        const Clip* clip = this->clip(clipId);
        if (!clip) {
            continue;
        }
        const QString clipSubject = QStringLiteral("clip:%1").arg(QString::number(clipId.value));
        if (!clip->timelineRange.isValid() || !clip->sourceRange.isValid()) {
            report.success = false;
            ++report.invalidClipCount;
            addIssue(report, QStringLiteral("invalid_clip_range"), clipSubject,
                     QStringLiteral("Clip has an invalid source or timeline range"));
        }
        if (!hasTrack(clip->trackId)) {
            report.success = false;
            ++report.invalidClipCount;
            addIssue(report, QStringLiteral("missing_track"), clipSubject,
                     QStringLiteral("Clip references a missing track"));
        }
        if (!hasSource(clip->sourceId)) {
            report.success = false;
            ++report.invalidClipCount;
            addIssue(report, QStringLiteral("missing_source"), clipSubject,
                     QStringLiteral("Clip references a missing source"));
        }
        if (clip->linkedGroupId != 0 && !linkGroup(clip->linkedGroupId)) {
            report.success = false;
            ++report.brokenLinkGroupCount;
            addIssue(report, QStringLiteral("broken_link_group"), clipSubject,
                     QStringLiteral("Clip references a missing link group"));
        }
    }

    for (const TrackId& trackId : impl_->tracks.keys()) {
        const Track* track = this->track(trackId);
        if (!track) {
            continue;
        }
        QVector<ClipId> sortedClipIds = impl_->clipsOnTrack(trackId);
        for (int i = 1; i < sortedClipIds.size(); ++i) {
            const Clip* prev = this->clip(sortedClipIds[i - 1]);
            const Clip* current = this->clip(sortedClipIds[i]);
            if (!prev || !current || !prev->timelineRange.isValid() || !current->timelineRange.isValid()) {
                continue;
            }
            const bool overlaps = prev->timelineRange.end() >= current->timelineRange.start() &&
                                  prev->timelineRange.end() + 1 != current->timelineRange.start();
            if (overlaps) {
                report.success = false;
                ++report.overlappingClipCount;
                addIssue(report,
                         QStringLiteral("overlapping_clip"),
                         QStringLiteral("track:%1").arg(QString::number(trackId.value)),
                         QStringLiteral("Track contains overlapping clips %1 and %2")
                             .arg(QString::number(prev->id.value), QString::number(current->id.value)));
            }
        }
    }

    for (const TransitionId& transitionId : impl_->transitions.keys()) {
        const Transition* transition = this->transition(transitionId);
        if (!transition) {
            continue;
        }
        const QString transitionSubject = QStringLiteral("transition:%1").arg(QString::number(transitionId.value));
        if (!hasTrack(transition->trackId) ||
            !hasClip(transition->leftClipId) ||
            !hasClip(transition->rightClipId)) {
            report.success = false;
            ++report.orphanTransitionCount;
            addIssue(report, QStringLiteral("orphan_transition"), transitionSubject,
                     QStringLiteral("Transition references a missing track or clip"));
            continue;
        }
        const Clip* left = this->clip(transition->leftClipId);
        const Clip* right = this->clip(transition->rightClipId);
        if (!left || !right || left->trackId != transition->trackId || right->trackId != transition->trackId) {
            report.success = false;
            ++report.orphanTransitionCount;
            addIssue(report, QStringLiteral("orphan_transition"), transitionSubject,
                     QStringLiteral("Transition clips are not attached to the transition track"));
        }
    }

    for (const ClipLinkGroup& group : linkGroups()) {
        for (const ClipId& memberId : group.members) {
            const Clip* clip = this->clip(memberId);
            if (!clip) {
                report.success = false;
                ++report.brokenLinkGroupCount;
                addIssue(report,
                         QStringLiteral("broken_link_group"),
                         QStringLiteral("link_group:%1").arg(QString::number(group.id)),
                         QStringLiteral("Link group references a missing clip %1").arg(QString::number(memberId.value)));
                continue;
            }
            if (clip->linkedGroupId != group.id) {
                report.success = false;
                ++report.brokenLinkGroupCount;
                addIssue(report,
                         QStringLiteral("broken_link_group"),
                         QStringLiteral("link_group:%1").arg(QString::number(group.id)),
                         QStringLiteral("Clip %1 does not point back to its link group").arg(QString::number(memberId.value)));
            }
        }
    }

    if (report.issues.isEmpty()) {
        report.success = true;
    }
    return report;
}

void NLEProjectStore::clear()
{
    impl_->clear();
}

QJsonObject NLEProjectStore::toJson() const
{
    QJsonArray sequences;
    for (const SequenceId& id : sequenceIds()) {
        if (const Sequence* sequence = this->sequence(id)) {
            sequences.append(sequenceToJson(*sequence));
        }
    }

    QJsonArray tracks;
    for (const TrackId& id : impl_->tracks.keys()) {
        if (const Track* track = this->track(id)) {
            tracks.append(trackToJson(*track));
        }
    }

    QJsonArray clips;
    for (const ClipId& id : impl_->clips.keys()) {
        if (const Clip* clip = this->clip(id)) {
            clips.append(clipToJson(*clip));
        }
    }

    QJsonArray markers;
    for (const MarkerId& id : impl_->markers.keys()) {
        if (const Marker* marker = this->marker(id)) {
            markers.append(markerToJson(*marker));
        }
    }

    QJsonArray transitions;
    for (const TransitionId& id : impl_->transitions.keys()) {
        if (const Transition* transition = this->transition(id)) {
            transitions.append(transitionToJson(*transition));
        }
    }

    QJsonArray sources;
    for (const SourceId& id : impl_->sources.keys()) {
        if (const SourceRef* source = this->source(id)) {
            sources.append(sourceRefToJson(*source));
        }
    }

    QJsonArray linkGroups;
    for (const auto& group : impl_->linkGroups) {
        linkGroups.append(linkGroupToJson(group));
    }

    return QJsonObject{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("nextSequenceId"), QString::number(impl_->nextSequenceId)},
        {QStringLiteral("nextTrackId"), QString::number(impl_->nextTrackId)},
        {QStringLiteral("nextClipId"), QString::number(impl_->nextClipId)},
        {QStringLiteral("nextMarkerId"), QString::number(impl_->nextMarkerId)},
        {QStringLiteral("nextTransitionId"), QString::number(impl_->nextTransitionId)},
        {QStringLiteral("nextSourceId"), QString::number(impl_->nextSourceId)},
        {QStringLiteral("nextLinkGroupId"), QString::number(impl_->nextLinkGroupId)},
        {QStringLiteral("sequences"), sequences},
        {QStringLiteral("tracks"), tracks},
        {QStringLiteral("clips"), clips},
        {QStringLiteral("markers"), markers},
        {QStringLiteral("transitions"), transitions},
        {QStringLiteral("sources"), sources},
        {QStringLiteral("linkGroups"), linkGroups}
    };
}

bool NLEProjectStore::loadFromJson(const QJsonObject& json)
{
    clear();

    impl_->nextSequenceId = jsonValueToUInt64(json.value(QStringLiteral("nextSequenceId")), 1);
    impl_->nextTrackId = jsonValueToUInt64(json.value(QStringLiteral("nextTrackId")), 1);
    impl_->nextClipId = jsonValueToUInt64(json.value(QStringLiteral("nextClipId")), 1);
    impl_->nextMarkerId = jsonValueToUInt64(json.value(QStringLiteral("nextMarkerId")), 1);
    impl_->nextTransitionId = jsonValueToUInt64(json.value(QStringLiteral("nextTransitionId")), 1);
    impl_->nextSourceId = jsonValueToUInt64(json.value(QStringLiteral("nextSourceId")), 1);
    impl_->nextLinkGroupId = jsonValueToUInt64(json.value(QStringLiteral("nextLinkGroupId")), 1);

    for (const QJsonValue& value : json.value(QStringLiteral("sequences")).toArray()) {
        const Sequence sequence = sequenceFromJson(value.toObject());
        impl_->sequences.insert(sequence.id, sequence);
    }
    for (const QJsonValue& value : json.value(QStringLiteral("tracks")).toArray()) {
        const Track track = trackFromJson(value.toObject());
        impl_->tracks.insert(track.id, track);
    }
    for (const QJsonValue& value : json.value(QStringLiteral("clips")).toArray()) {
        const Clip clip = clipFromJson(value.toObject());
        impl_->clips.insert(clip.id, clip);
    }
    for (const QJsonValue& value : json.value(QStringLiteral("markers")).toArray()) {
        const Marker marker = markerFromJson(value.toObject());
        impl_->markers.insert(marker.id, marker);
    }
    for (const QJsonValue& value : json.value(QStringLiteral("transitions")).toArray()) {
        const Transition transition = transitionFromJson(value.toObject());
        impl_->transitions.insert(transition.id, transition);
    }
    for (const QJsonValue& value : json.value(QStringLiteral("sources")).toArray()) {
        const SourceRef source = sourceRefFromJson(value.toObject());
        impl_->sources.insert(source.sourceId, source);
    }
    for (const QJsonValue& value : json.value(QStringLiteral("linkGroups")).toArray()) {
        const ClipLinkGroup group = linkGroupFromJson(value.toObject());
        impl_->linkGroups.insert(group.id, group);
    }

    for (auto it = impl_->clips.begin(); it != impl_->clips.end(); ++it) {
        const quint64 groupId = it.value().linkedGroupId;
        if (groupId == 0) {
            continue;
        }
        auto groupIt = impl_->linkGroups.find(groupId);
        if (groupIt == impl_->linkGroups.end()) {
            ClipLinkGroup group;
            group.id = groupId;
            group.members.push_back(it.key());
            impl_->linkGroups.insert(groupId, group);
            continue;
        }
        if (!groupIt->members.contains(it.key())) {
            groupIt->members.push_back(it.key());
        }
    }

    return true;
}

quint64 NLEProjectStore::createLinkGroup(bool videoAudioLinked,
                                         bool selectionLinked,
                                         bool trimLinked)
{
    ClipLinkGroup group;
    group.id = impl_->nextLinkGroupId++;
    group.videoAudioLinked = videoAudioLinked;
    group.moveLinked = true;
    group.selectionLinked = selectionLinked;
    group.trimLinked = trimLinked;
    impl_->linkGroups.insert(group.id, group);
    return group.id;
}

bool NLEProjectStore::addClipToLinkGroup(const ClipId& clipId, quint64 groupId)
{
    auto clip = findClip(impl_->clips, clipId);
    auto groupIt = impl_->linkGroups.find(groupId);
    if (!clip || groupIt == impl_->linkGroups.end()) {
        return false;
    }
    if (clip->linkedGroupId != 0 && clip->linkedGroupId != groupId) {
        removeClipFromLinkGroup(clipId);
    }
    clip->linkedGroupId = groupId;
    if (!groupIt->members.contains(clipId)) {
        groupIt->members.push_back(clipId);
    }
    return true;
}

bool NLEProjectStore::removeClipFromLinkGroup(const ClipId& clipId)
{
    auto clip = findClip(impl_->clips, clipId);
    if (!clip || clip->linkedGroupId == 0) {
        return false;
    }
    const quint64 groupId = clip->linkedGroupId;
    clip->linkedGroupId = 0;
    auto groupIt = impl_->linkGroups.find(groupId);
    if (groupIt != impl_->linkGroups.end()) {
        groupIt->members.removeAll(clipId);
        if (groupIt->members.isEmpty()) {
            impl_->linkGroups.remove(groupId);
        }
    }
    return true;
}

bool NLEProjectStore::setLinkGroupFlags(quint64 groupId,
                                        bool videoAudioLinked,
                                        bool moveLinked,
                                        bool selectionLinked,
                                        bool trimLinked)
{
    auto groupIt = impl_->linkGroups.find(groupId);
    if (groupIt == impl_->linkGroups.end()) {
        return false;
    }
    groupIt->videoAudioLinked = videoAudioLinked;
    groupIt->moveLinked = moveLinked;
    groupIt->selectionLinked = selectionLinked;
    groupIt->trimLinked = trimLinked;
    return true;
}

ClipLinkGroup* NLEProjectStore::linkGroup(quint64 groupId)
{
    auto it = impl_->linkGroups.find(groupId);
    return it == impl_->linkGroups.end() ? nullptr : &it.value();
}

const ClipLinkGroup* NLEProjectStore::linkGroup(quint64 groupId) const
{
    auto it = impl_->linkGroups.find(groupId);
    return it == impl_->linkGroups.end() ? nullptr : &it.value();
}

QVector<ClipLinkGroup> NLEProjectStore::linkGroups() const
{
    QVector<ClipLinkGroup> groups;
    groups.reserve(impl_->linkGroups.size());
    for (const auto& group : impl_->linkGroups) {
        groups.push_back(group);
    }
    std::sort(groups.begin(), groups.end(), [](const ClipLinkGroup& a, const ClipLinkGroup& b) {
        return a.id < b.id;
    });
    return groups;
}

SequenceEditor::SequenceEditor(NLEProjectStore* store)
    : store_(store)
{
}

SequenceEditor::~SequenceEditor() = default;

void SequenceEditor::setStore(NLEProjectStore* store)
{
    store_ = store;
}

NLEProjectStore* SequenceEditor::store()
{
    return store_;
}

const NLEProjectStore* SequenceEditor::store() const
{
    return store_;
}

EditResult SequenceEditor::insertClip(const SequenceId& sequenceId,
                                      const TrackId& trackId,
                                      const ClipDraft& draft)
{
    EditResult result;
    if (!store_) {
        result.message = QStringLiteral("No NLE store attached");
        return result;
    }
    const ClipId clipId = store_->addClip(sequenceId, trackId, draft);
    result.success = clipId.isValid();
    result.message = result.success ? QStringLiteral("Clip inserted")
                                    : QStringLiteral("Failed to insert clip");
    if (result.success) {
        result.touchedSequences.push_back(sequenceId);
        result.touchedTracks.push_back(trackId);
        result.touchedClips.push_back(clipId);
    }
    return result;
}

EditResult SequenceEditor::overwriteClip(const SequenceId& sequenceId,
                                         const TrackId& trackId,
                                         const ClipDraft& draft)
{
    EditResult result;
    if (!store_) {
        result.message = QStringLiteral("No NLE store attached");
        return result;
    }
    const ClipId clipId = store_->overwriteClip(sequenceId, trackId, draft);
    result.success = clipId.isValid();
    result.message = result.success ? QStringLiteral("Clip overwritten")
                                    : QStringLiteral("Failed to overwrite clip");
    if (result.success) {
        result.touchedSequences.push_back(sequenceId);
        result.touchedTracks.push_back(trackId);
        result.touchedClips.push_back(clipId);
    }
    return result;
}

EditResult SequenceEditor::trimClip(const ClipId& clipId,
                                    const FrameRange& newSourceRange,
                                    TrimMode mode)
{
    EditResult result;
    if (!store_) {
        result.message = QStringLiteral("No NLE store attached");
        return result;
    }
    const Clip* clip = store_->clip(clipId);
    if (!clip) {
        result.message = QStringLiteral("Clip not found");
        return result;
    }
    LinkingService linking(store_);
    const auto propagation = linking.propagateTrimLink(clipId, newSourceRange, mode);
    const bool ok = propagation.success;
    result.success = ok;
    result.message = ok ? QStringLiteral("Clip trimmed")
                        : QStringLiteral("Failed to trim clip");
    if (ok) {
        for (const ClipId& touchedClipId : propagation.touchedClips) {
            result.touchedClips.push_back(touchedClipId);
            if (const Clip* touchedClip = store_->clip(touchedClipId)) {
                appendUnique(result.touchedTracks, touchedClip->trackId);
                if (const Track* track = store_->track(touchedClip->trackId)) {
                    appendUnique(result.touchedSequences, track->ownerSequenceId);
                }
            }
        }
        for (const QString& warning : propagation.warnings) {
            result.warnings.push_back(warning);
        }
    }
    return result;
}

EditResult SequenceEditor::rippleDelete(const ClipId& clipId)
{
    EditResult result;
    if (!store_) {
        result.message = QStringLiteral("No NLE store attached");
        return result;
    }
    const Clip* clip = store_->clip(clipId);
    if (!clip) {
        result.message = QStringLiteral("Clip not found");
        return result;
    }
    const bool ok = store_->rippleDelete(clipId);
    result.success = ok;
    result.message = ok ? QStringLiteral("Clip ripple-deleted")
                        : QStringLiteral("Failed to ripple-delete clip");
    if (ok) {
        result.touchedClips.push_back(clipId);
        result.touchedTracks.push_back(clip->trackId);
        if (const Track* track = store_->track(clip->trackId)) {
            result.touchedSequences.push_back(track->ownerSequenceId);
        }
    }
    return result;
}

EditResult SequenceEditor::rollTrim(const ClipId& leftClipId,
                                    const ClipId& rightClipId,
                                    const FramePosition& boundary)
{
    EditResult result;
    if (!store_) {
        result.message = QStringLiteral("No NLE store attached");
        return result;
    }
    const Clip* left = store_->clip(leftClipId);
    const Clip* right = store_->clip(rightClipId);
    if (!left || !right) {
        result.message = QStringLiteral("Clip not found");
        return result;
    }
    const bool ok = store_->rollTrim(leftClipId, rightClipId, boundary);
    result.success = ok;
    result.message = ok ? QStringLiteral("Roll trim applied")
                        : QStringLiteral("Failed to roll trim");
    if (ok) {
        result.touchedClips = {leftClipId, rightClipId};
        result.touchedTracks.push_back(left->trackId);
        if (const Track* track = store_->track(left->trackId)) {
            result.touchedSequences.push_back(track->ownerSequenceId);
        }
    }
    return result;
}

EditResult SequenceEditor::slipClip(const ClipId& clipId, const FramePosition& newSourceStart)
{
    EditResult result;
    if (!store_) {
        result.message = QStringLiteral("No NLE store attached");
        return result;
    }
    const Clip* clip = store_->clip(clipId);
    if (!clip) {
        result.message = QStringLiteral("Clip not found");
        return result;
    }
    const FrameRange newSourceRange = FrameRange::fromDuration(newSourceStart.framePosition(), clip->sourceRange.duration());
    LinkingService linking(store_);
    const auto propagation = linking.propagateTrimLink(clipId, newSourceRange, TrimMode::Slip);
    const bool ok = propagation.success;
    result.success = ok;
    result.message = ok ? QStringLiteral("Clip slipped")
                        : QStringLiteral("Failed to slip clip");
    if (ok) {
        for (const ClipId& touchedClipId : propagation.touchedClips) {
            result.touchedClips.push_back(touchedClipId);
            if (const Clip* touchedClip = store_->clip(touchedClipId)) {
                appendUnique(result.touchedTracks, touchedClip->trackId);
                if (const Track* track = store_->track(touchedClip->trackId)) {
                    appendUnique(result.touchedSequences, track->ownerSequenceId);
                }
            }
        }
        for (const QString& warning : propagation.warnings) {
            result.warnings.push_back(warning);
        }
    }
    return result;
}

EditResult SequenceEditor::slideClip(const ClipId& clipId, const FramePosition& newTimelineStart)
{
    EditResult result;
    if (!store_) {
        result.message = QStringLiteral("No NLE store attached");
        return result;
    }
    const Clip* clip = store_->clip(clipId);
    if (!clip) {
        result.message = QStringLiteral("Clip not found");
        return result;
    }
    LinkingService linking(store_);
    const auto propagation = linking.propagateMoveLink(clipId, newTimelineStart);
    const bool ok = propagation.success;
    result.success = ok;
    result.message = ok ? QStringLiteral("Clip slid")
                        : QStringLiteral("Failed to slide clip");
    if (ok) {
        for (const ClipId& touchedClipId : propagation.touchedClips) {
            result.touchedClips.push_back(touchedClipId);
            if (const Clip* touchedClip = store_->clip(touchedClipId)) {
                appendUnique(result.touchedTracks, touchedClip->trackId);
                if (const Track* track = store_->track(touchedClip->trackId)) {
                    appendUnique(result.touchedSequences, track->ownerSequenceId);
                }
            }
        }
        for (const QString& warning : propagation.warnings) {
            result.warnings.push_back(warning);
        }
    }
    return result;
}

EditResult SequenceEditor::selectClip(const ClipId& clipId, bool selected)
{
    EditResult result;
    if (!store_) {
        result.message = QStringLiteral("No NLE store attached");
        return result;
    }
    const Clip* clip = store_->clip(clipId);
    if (!clip) {
        result.message = QStringLiteral("Clip not found");
        return result;
    }
    LinkingService linking(store_);
    const auto propagation = linking.propagateSelectionLink(clipId, selected);
    const bool ok = propagation.success;
    result.success = ok;
    result.message = ok ? QStringLiteral("Clip selection updated")
                        : QStringLiteral("Failed to update clip selection");
    if (ok) {
        for (const ClipId& touchedClipId : propagation.touchedClips) {
            result.touchedClips.push_back(touchedClipId);
            if (const Clip* touchedClip = store_->clip(touchedClipId)) {
                appendUnique(result.touchedTracks, touchedClip->trackId);
                if (const Track* track = store_->track(touchedClip->trackId)) {
                    appendUnique(result.touchedSequences, track->ownerSequenceId);
                }
            }
        }
        for (const QString& warning : propagation.warnings) {
            result.warnings.push_back(warning);
        }
    }
    return result;
}

ClipResolver::ClipResolver(const NLEProjectStore* store)
    : store_(store)
{
}

void ClipResolver::setStore(const NLEProjectStore* store)
{
    store_ = store;
}

const NLEProjectStore* ClipResolver::store() const
{
    return store_;
}

ClipResolution ClipResolver::resolveClip(const ClipId& clipId) const
{
    ClipResolution resolution;
    resolution.clipId = clipId;

    if (!store_) {
        resolution.diagnostic = QStringLiteral("No NLE store attached");
        return resolution;
    }

    const Clip* clip = store_->clip(clipId);
    if (!clip) {
        resolution.diagnostic = QStringLiteral("Clip not found");
        return resolution;
    }

    resolution.sourceRange = clip->sourceRange;
    resolution.effectiveRange = clip->sourceRange;

    const SourceRef* source = store_->source(clip->sourceId);
    if (!source) {
        resolution.diagnostic = QStringLiteral("Source not registered");
        return resolution;
    }

    resolution.source = *source;
    resolution.online = source->online;
    resolution.useProxy = source->proxyAvailable && source->useProxy && !source->proxyUri.isEmpty();
    if (!source->online) {
        resolution.diagnostic = QStringLiteral("Source is offline");
        return resolution;
    }

    if (source->availableRange.isValid()) {
        resolution.effectiveRange = clip->sourceRange.clipped(source->availableRange);
        if (resolution.effectiveRange != clip->sourceRange) {
            resolution.diagnostic = QStringLiteral("Source range clipped to available range");
        }
    }

    return resolution;
}

ConformService::ConformService(const NLEProjectStore* store)
    : store_(store)
{
}

void ConformService::setStore(const NLEProjectStore* store)
{
    store_ = store;
}

const NLEProjectStore* ConformService::store() const
{
    return store_;
}

ConformReport ConformService::conformSequence(const SequenceId& sequenceId) const
{
    ConformReport report;
    if (!store_) {
        report.warnings.push_back(QStringLiteral("No NLE store attached"));
        return report;
    }

    const Sequence* sequence = store_->sequence(sequenceId);
    if (!sequence) {
        report.warnings.push_back(QStringLiteral("Sequence not found"));
        return report;
    }

    report.success = true;
    const ClipResolver resolver(store_);
    for (const ClipId& clipId : store_->clipIdsInSequence(sequenceId)) {
        const Clip* clip = store_->clip(clipId);
        if (!clip) {
            continue;
        }
        const ClipResolution resolution = resolver.resolveClip(clipId);
        if (!resolution.online) {
            report.unresolvedClips.push_back(clipId);
            report.warnings.push_back(
                QStringLiteral("Clip %1 is unresolved: %2").arg(clipId.toString(), resolution.diagnostic));
            continue;
        }
        if (resolution.effectiveRange != clip->sourceRange) {
            report.updatedClips.push_back(clipId);
            report.warnings.push_back(
                QStringLiteral("Clip %1 source range needs conform").arg(clipId.toString()));
        }
    }
    report.updatedSequences.push_back(sequence->id);
    return report;
}

ConformReport ConformService::conformAll() const
{
    ConformReport aggregate;
    if (!store_) {
        aggregate.warnings.push_back(QStringLiteral("No NLE store attached"));
        return aggregate;
    }

    aggregate.success = true;
    for (const SequenceId& sequenceId : store_->sequenceIds()) {
        const ConformReport report = conformSequence(sequenceId);
        aggregate.updatedSequences += report.updatedSequences;
        aggregate.updatedClips += report.updatedClips;
        aggregate.unresolvedClips += report.unresolvedClips;
        aggregate.warnings += report.warnings;
    }
    return aggregate;
}

LinkingService::LinkingService(NLEProjectStore* store)
    : store_(store)
{
}

void LinkingService::setStore(NLEProjectStore* store)
{
    store_ = store;
}

NLEProjectStore* LinkingService::store()
{
    return store_;
}

const NLEProjectStore* LinkingService::store() const
{
    return store_;
}

quint64 LinkingService::createLinkGroup(bool videoAudioLinked,
                                        bool selectionLinked,
                                        bool trimLinked)
{
    return store_ ? store_->createLinkGroup(videoAudioLinked, selectionLinked, trimLinked) : 0;
}

bool LinkingService::addClipToLinkGroup(const ClipId& clipId, quint64 groupId)
{
    return store_ ? store_->addClipToLinkGroup(clipId, groupId) : false;
}

bool LinkingService::removeClipFromLinkGroup(const ClipId& clipId)
{
    return store_ ? store_->removeClipFromLinkGroup(clipId) : false;
}

bool LinkingService::setLinkGroupFlags(quint64 groupId,
                                       bool videoAudioLinked,
                                       bool moveLinked,
                                       bool selectionLinked,
                                       bool trimLinked)
{
    return store_ ? store_->setLinkGroupFlags(groupId, videoAudioLinked, moveLinked, selectionLinked, trimLinked) : false;
}

QVector<ClipId> LinkingService::linkedClips(const ClipId& clipId) const
{
    if (!store_) {
        return {};
    }
    const Clip* clip = store_->clip(clipId);
    if (!clip || clip->linkedGroupId == 0) {
        return {};
    }
    const ClipLinkGroup* group = store_->linkGroup(clip->linkedGroupId);
    if (!group) {
        return {};
    }
    QVector<ClipId> result = group->members;
    result.removeAll(clipId);
    return result;
}

bool LinkingService::propagateSelectionLink(const ClipId& sourceClipId)
{
    if (!store_) {
        return false;
    }
    const Clip* clip = store_->clip(sourceClipId);
    if (!clip || clip->linkedGroupId == 0) {
        return false;
    }
    const ClipLinkGroup* group = store_->linkGroup(clip->linkedGroupId);
    return group && group->selectionLinked;
}

bool LinkingService::propagateTrimLink(const ClipId& sourceClipId)
{
    if (!store_) {
        return false;
    }
    const Clip* clip = store_->clip(sourceClipId);
    if (!clip || clip->linkedGroupId == 0) {
        return false;
    }
    const ClipLinkGroup* group = store_->linkGroup(clip->linkedGroupId);
    return group && group->trimLinked;
}

bool LinkingService::propagateMoveLink(const ClipId& sourceClipId)
{
    if (!store_) {
        return false;
    }
    const Clip* clip = store_->clip(sourceClipId);
    if (!clip || clip->linkedGroupId == 0) {
        return false;
    }
    const ClipLinkGroup* group = store_->linkGroup(clip->linkedGroupId);
    return group && group->moveLinked;
}

LinkingService::LinkPropagationResult LinkingService::propagateSelectionLink(const ClipId& sourceClipId,
                                                                             bool selected)
{
    LinkPropagationResult result;
    if (!store_) {
        result.warnings.push_back(QStringLiteral("No NLE store attached"));
        return result;
    }
    Clip* clip = store_->clip(sourceClipId);
    if (!clip) {
        result.warnings.push_back(QStringLiteral("Clip not found"));
        return result;
    }

    clip->selected = selected;
    result.success = true;
    result.touchedClips.push_back(sourceClipId);

    if (clip->linkedGroupId == 0) {
        return result;
    }
    const ClipLinkGroup* group = store_->linkGroup(clip->linkedGroupId);
    if (!group || !group->selectionLinked) {
        if (!group) {
            result.warnings.push_back(QStringLiteral("Broken link group"));
        }
        return result;
    }

    for (const ClipId& peerId : group->members) {
        if (peerId == sourceClipId) {
            continue;
        }
        if (Clip* peer = store_->clip(peerId)) {
            peer->selected = selected;
            result.touchedClips.push_back(peerId);
        }
    }
    return result;
}

LinkingService::LinkPropagationResult LinkingService::propagateMoveLink(const ClipId& sourceClipId,
                                                                        const FramePosition& newTimelineStart)
{
    LinkPropagationResult result;
    if (!store_) {
        result.warnings.push_back(QStringLiteral("No NLE store attached"));
        return result;
    }
    Clip* sourceClip = store_->clip(sourceClipId);
    if (!sourceClip) {
        result.warnings.push_back(QStringLiteral("Clip not found"));
        return result;
    }

    const Clip sourceBefore = *sourceClip;
    if (!store_->slideClip(sourceClipId, newTimelineStart)) {
        result.warnings.push_back(QStringLiteral("Failed to move source clip"));
        return result;
    }

    result.success = true;
    result.touchedClips.push_back(sourceClipId);

    if (sourceBefore.linkedGroupId == 0) {
        return result;
    }
    const ClipLinkGroup* group = store_->linkGroup(sourceBefore.linkedGroupId);
    if (!group || !group->moveLinked) {
        if (!group) {
            result.warnings.push_back(QStringLiteral("Broken link group"));
        }
        return result;
    }

    const int64_t delta = newTimelineStart.framePosition() - sourceBefore.timelineRange.start();
    for (const ClipId& peerId : group->members) {
        if (peerId == sourceClipId) {
            continue;
        }
        Clip* peer = store_->clip(peerId);
        if (!peer || !peer->timelineRange.isValid()) {
            continue;
        }
        const FramePosition peerNewStart(peer->timelineRange.start() + delta);
        if (store_->slideClip(peerId, peerNewStart)) {
            result.touchedClips.push_back(peerId);
        }
        else {
            result.warnings.push_back(QStringLiteral("Failed to move linked clip %1").arg(QString::number(peerId.value)));
        }
    }

    return result;
}

LinkingService::LinkPropagationResult LinkingService::propagateTrimLink(const ClipId& sourceClipId,
                                                                        const FrameRange& newSourceRange,
                                                                        TrimMode mode)
{
    LinkPropagationResult result;
    if (!store_) {
        result.warnings.push_back(QStringLiteral("No NLE store attached"));
        return result;
    }
    Clip* sourceClip = store_->clip(sourceClipId);
    if (!sourceClip) {
        result.warnings.push_back(QStringLiteral("Clip not found"));
        return result;
    }

    const Clip sourceBefore = *sourceClip;
    if (!store_->trimClip(sourceClipId, newSourceRange, mode)) {
        result.warnings.push_back(QStringLiteral("Failed to trim source clip"));
        return result;
    }

    result.success = true;
    result.touchedClips.push_back(sourceClipId);

    if (sourceBefore.linkedGroupId == 0) {
        return result;
    }
    const ClipLinkGroup* group = store_->linkGroup(sourceBefore.linkedGroupId);
    if (!group || !group->trimLinked) {
        if (!group) {
            result.warnings.push_back(QStringLiteral("Broken link group"));
        }
        return result;
    }

    const int64_t startDelta = newSourceRange.start() - sourceBefore.sourceRange.start();
    const int64_t endDelta = newSourceRange.end() - sourceBefore.sourceRange.end();

    for (const ClipId& peerId : group->members) {
        if (peerId == sourceClipId) {
            continue;
        }
        Clip* peer = store_->clip(peerId);
        if (!peer || !peer->sourceRange.isValid()) {
            continue;
        }
        const FrameRange peerRange = remapRangeByDelta(peer->sourceRange, startDelta, endDelta);
        if (!peerRange.isValid()) {
            result.warnings.push_back(QStringLiteral("Linked trim produced an invalid range for clip %1").arg(QString::number(peerId.value)));
            continue;
        }
        if (store_->trimClip(peerId, peerRange, mode)) {
            result.touchedClips.push_back(peerId);
        }
        else {
            result.warnings.push_back(QStringLiteral("Failed to trim linked clip %1").arg(QString::number(peerId.value)));
        }
    }

    return result;
}

} // namespace ArtifactCore::NLE
