module;

#include <QJsonArray>
#include <QJsonObject>
#include <QColor>
#include <QString>
#include <QVector>
#include <QRegularExpression>
#include <QStringList>
#include <cmath>
#include <algorithm>
#include <QFile>
#include <QSaveFile>

module NLE.OTIO;

import NLE.Core;
import Frame.Range;

namespace ArtifactCore::NLE {
namespace {

QJsonObject rationalTime(const qint64 value, const double rate)
{
    return QJsonObject{
        {QStringLiteral("OTIO_SCHEMA"), QStringLiteral("RationalTime.1")},
        {QStringLiteral("value"), value},
        {QStringLiteral("rate"), rate}
    };
}

QJsonObject timeRange(const FrameRange& range, const double rate)
{
    return QJsonObject{
        {QStringLiteral("OTIO_SCHEMA"), QStringLiteral("TimeRange.1")},
        {QStringLiteral("start_time"), rationalTime(range.start(), rate)},
        {QStringLiteral("duration"), rationalTime(range.duration(), rate)}
    };
}

QString trackKind(const TrackKind kind)
{
    switch (kind) {
    case TrackKind::Audio: return QStringLiteral("Audio");
    case TrackKind::Subtitle: return QStringLiteral("Subtitle");
    case TrackKind::Data: return QStringLiteral("Data");
    default: return QStringLiteral("Video");
    }
}

TrackKind parseTrackKind(const QString& value)
{
    if (value.compare(QStringLiteral("Audio"), Qt::CaseInsensitive) == 0) return TrackKind::Audio;
    if (value.compare(QStringLiteral("Subtitle"), Qt::CaseInsensitive) == 0) return TrackKind::Subtitle;
    if (value.compare(QStringLiteral("Data"), Qt::CaseInsensitive) == 0) return TrackKind::Data;
    if (value.compare(QStringLiteral("Video"), Qt::CaseInsensitive) == 0) return TrackKind::Video;
    return TrackKind::Video;
}

QString transitionName(const TransitionKind kind)
{
    switch (kind) {
    case TransitionKind::Cut: return QStringLiteral("Cut");
    case TransitionKind::Dissolve: return QStringLiteral("Dissolve");
    case TransitionKind::Crossfade: return QStringLiteral("Crossfade");
    case TransitionKind::Wipe: return QStringLiteral("Wipe");
    case TransitionKind::Slide: return QStringLiteral("Slide");
    case TransitionKind::Zoom: return QStringLiteral("Zoom");
    case TransitionKind::GlitchDisplace: return QStringLiteral("GlitchDisplace");
    case TransitionKind::Spin: return QStringLiteral("Spin");
    case TransitionKind::LinearWipe: return QStringLiteral("LinearWipe");
    case TransitionKind::RadialWipe: return QStringLiteral("RadialWipe");
    case TransitionKind::Flip: return QStringLiteral("Flip");
    case TransitionKind::Cube: return QStringLiteral("Cube");
    case TransitionKind::Doors: return QStringLiteral("Doors");
    case TransitionKind::LightLeak: return QStringLiteral("LightLeak");
    case TransitionKind::GradientWipe: return QStringLiteral("GradientWipe");
    case TransitionKind::IrisWipe: return QStringLiteral("IrisWipe");
    case TransitionKind::BlockDissolve: return QStringLiteral("BlockDissolve");
    }
    return QStringLiteral("Crossfade");
}

TransitionKind parseTransitionKind(const QString& value, const QJsonObject& metadata = {})
{
    const QVariant artifactKind =
        metadata.value(QStringLiteral("artifactKind")).toVariant();
    bool kindOk = false;
    const int kindInt = artifactKind.toInt(&kindOk);
    if (kindOk && kindInt >= 0 &&
        kindInt <= static_cast<int>(TransitionKind::BlockDissolve)) {
        return static_cast<TransitionKind>(kindInt);
    }
    if (value.compare(QStringLiteral("Cut"), Qt::CaseInsensitive) == 0) return TransitionKind::Cut;
    if (value.compare(QStringLiteral("Dissolve"), Qt::CaseInsensitive) == 0) return TransitionKind::Dissolve;
    if (value.compare(QStringLiteral("SMPTE_Dissolve"), Qt::CaseInsensitive) == 0) {
        return TransitionKind::Dissolve;
    }
    if (value.compare(QStringLiteral("Crossfade"), Qt::CaseInsensitive) == 0 ||
        value.compare(QStringLiteral("CrossDissolve"), Qt::CaseInsensitive) == 0) {
        return TransitionKind::Crossfade;
    }
    for (int kind = 0; kind <= static_cast<int>(TransitionKind::BlockDissolve); ++kind) {
        if (transitionName(static_cast<TransitionKind>(kind))
                .compare(value, Qt::CaseInsensitive) == 0) {
            return static_cast<TransitionKind>(kind);
        }
    }
    return TransitionKind::Crossfade;
}

constexpr int kMarkerPaletteSize = 9;

struct MarkerColorEntry {
    const char* name;
    QColor color;
};

const MarkerColorEntry& markerPalette(int index)
{
    static const MarkerColorEntry palette[kMarkerPaletteSize] = {
        {"RED", QColor(255, 0, 0)},
        {"GREEN", QColor(0, 255, 0)},
        {"BLUE", QColor(0, 0, 255)},
        {"YELLOW", QColor(255, 255, 0)},
        {"PINK", QColor(255, 192, 203)},
        {"PURPLE", QColor(128, 0, 128)},
        {"ORANGE", QColor(255, 165, 0)},
        {"WHITE", QColor(255, 255, 255)},
        {"BLACK", QColor(0, 0, 0)},
    };
    return palette[index];
}

QString markerColorName(const QColor& color)
{
    if (!color.isValid()) {
        return QStringLiteral("YELLOW");
    }
    const QColor rgb = color.toRgb();
    const qreal saturation = rgb.saturationF();
    const qreal lightness = rgb.lightnessF();
    if (lightness <= 0.08) {
        return QStringLiteral("BLACK");
    }
    if (lightness >= 0.92 && saturation <= 0.05) {
        return QStringLiteral("WHITE");
    }
    const qreal hue = rgb.hueF();
    const MarkerColorEntry* best = &markerPalette(3);
    qreal bestDistance = -1.0;
    for (int i = 0; i < kMarkerPaletteSize; ++i) {
        const MarkerColorEntry& candidate = markerPalette(i);
        if (candidate.name == QStringLiteral("BLACK") ||
            candidate.name == QStringLiteral("WHITE")) {
            continue;
        }
        const qreal candidateHue = candidate.color.hueF();
        qreal distance = qAbs(hue - candidateHue);
        if (distance > 0.5) {
            distance = 1.0 - distance;
        }
        if (bestDistance < 0.0 || distance < bestDistance) {
            bestDistance = distance;
            best = &candidate;
        }
    }
    return QString::fromLatin1(best->name);
}

QColor parseMarkerColor(const QJsonObject& markerObject)
{
    const QString exact = markerObject.value(QStringLiteral("metadata"))
                              .toObject()
                              .value(QStringLiteral("artifactColor"))
                              .toString();
    if (!exact.isEmpty()) {
        const QColor exactColor(exact);
        if (exactColor.isValid()) {
            return exactColor;
        }
    }
    const QString name = markerObject.value(QStringLiteral("color")).toString();
    QColor color(name);
    if (!color.isValid()) {
        color = QColor(name.toLower());
    }
    return color.isValid() ? color : QColor(Qt::yellow);
}

bool hasLinearTimeWarp(const QJsonObject& clipObject, double& timeScalar)
{
    const QJsonArray effects = clipObject.value(QStringLiteral("effects")).toArray();
    for (const QJsonValue& effectValue : effects) {
        const QJsonObject effect = effectValue.toObject();
        if (effect.value(QStringLiteral("OTIO_SCHEMA")).toString()
                .startsWith(QStringLiteral("LinearTimeWarp"))) {
            timeScalar = effect.value(QStringLiteral("time_scalar")).toDouble(1.0);
            return true;
        }
    }
    return false;
}

QJsonArray exportSequenceTracks(const NLEProjectStore& store,
                                const Sequence& sequence);

enum class ImportChildResult {
    Imported,
    Skipped,
};

// Imports one track's children (clips/gaps/transitions/nested stacks) into an
// existing track. `cursor` advances past each child's timeline footprint.
ImportChildResult importTrackChildren(NLEProjectStore& store,
                                      const TrackId& trackId,
                                      const TimeBase& timeBase,
                                      const QJsonArray& children,
                                      qint64& cursor,
                                      QVector<QString>* warnings);

SequenceId importSequenceTracks(NLEProjectStore& store,
                                const QString& sequenceName,
                                const TimeBase& timeBase,
                                const QJsonArray& tracks,
                                QVector<QString>* warnings)
{
    const SequenceId sequenceId =
        store.createSequence(sequenceName, timeBase);
    for (const QJsonValue& trackValue : tracks) {
        const QJsonObject trackObject = trackValue.toObject();
        const TrackId trackId = store.createTrack(
            sequenceId,
            parseTrackKind(trackObject.value(QStringLiteral("kind")).toString()),
            trackObject.value(QStringLiteral("name")).toString());
        qint64 cursor = 0;
        importTrackChildren(store, trackId, timeBase,
                            trackObject.value(QStringLiteral("children")).toArray(),
                            cursor, warnings);
    }
    return sequenceId;
}

ImportChildResult importTrackChildren(NLEProjectStore& store,
                                      const TrackId& trackId,
                                      const TimeBase& timeBase,
                                      const QJsonArray& children,
                                      qint64& cursor,
                                      QVector<QString>* warnings)
{
    bool importedAny = false;
    ClipId previousClipId;
    QJsonObject pendingTransition;
    for (const QJsonValue& clipValue : children) {
        const QJsonObject clipObject = clipValue.toObject();
        const QString schema = clipObject.value(QStringLiteral("OTIO_SCHEMA")).toString();
        if (schema.startsWith(QStringLiteral("Gap."))) {
            const qint64 gapDuration = static_cast<qint64>(
                clipObject.value(QStringLiteral("duration")).toObject()
                    .value(QStringLiteral("value")).toDouble());
            cursor += qMax<qint64>(0, gapDuration);
            continue;
        }
        if (schema.startsWith(QStringLiteral("Transition."))) {
            pendingTransition = clipObject;
            continue;
        }

        const QJsonObject clipMetadata =
            clipObject.value(QStringLiteral("metadata")).toObject();

        if (schema.startsWith(QStringLiteral("Stack."))) {
            const SequenceId nestedSequenceId = importSequenceTracks(
                store,
                clipObject.value(QStringLiteral("name")).toString(),
                timeBase,
                clipObject.value(QStringLiteral("children")).toArray(),
                warnings);
            if (!nestedSequenceId.isValid()) {
                continue;
            }
            const QJsonObject artifactRange =
                clipMetadata.value(QStringLiteral("artifactTimelineRange")).toObject();
            const qint64 nestedStart =
                artifactRange.contains(QStringLiteral("start"))
                    ? artifactRange.value(QStringLiteral("start")).toVariant().toLongLong()
                    : cursor;
            const qint64 nestedDuration =
                artifactRange.contains(QStringLiteral("duration"))
                    ? artifactRange.value(QStringLiteral("duration")).toVariant().toLongLong()
                    : 1;
            ClipDraft draft;
            draft.nestedSequenceId = nestedSequenceId;
            draft.timelineRange = FrameRange::fromDuration(nestedStart, nestedDuration);
            draft.sourceRange = draft.timelineRange;
            draft.trimRange = draft.timelineRange;
            draft.name = clipObject.value(QStringLiteral("name")).toString();
            const ClipId nestedClipId = store.addClip(
                store.track(trackId) ? store.track(trackId)->ownerSequenceId
                                     : SequenceId{},
                trackId, draft);
            if (!nestedClipId.isValid()) {
                if (warnings) {
                    warnings->push_back(
                        QStringLiteral("Failed to import nested sequence clip"));
                }
            } else {
                importedAny = true;
            }
            cursor = qMax(cursor, nestedStart + nestedDuration);
            continue;
        }

        if (!schema.startsWith(QStringLiteral("Clip."))) {
            if (warnings) {
                warnings->push_back(
                    QStringLiteral("Skipped unsupported OTIO child in track"));
            }
            continue;
        }

        const QJsonObject sourceRangeObject =
            clipObject.value(QStringLiteral("source_range")).toObject();
        const QJsonObject sourceStart =
            sourceRangeObject.value(QStringLiteral("start_time")).toObject();
        const QJsonObject sourceDuration =
            sourceRangeObject.value(QStringLiteral("duration")).toObject();
        const qint64 sourceStartValue = static_cast<qint64>(
            sourceStart.value(QStringLiteral("value")).toDouble());
        const qint64 durationValue = static_cast<qint64>(
            sourceDuration.value(QStringLiteral("value")).toDouble());
        const QJsonObject media =
            clipObject.value(QStringLiteral("media_reference")).toObject();
        SourceRef source;
        source.uri = media.value(QStringLiteral("target_url")).toString();
        source.displayName = media.value(QStringLiteral("name")).toString();
        source.timeBase = timeBase;
        const SourceId sourceId = store.registerSource(source);
        ClipDraft draft;
        draft.sourceId = sourceId;
        draft.sourceRange = FrameRange::fromDuration(sourceStartValue, durationValue);
        draft.timelineRange = FrameRange::fromDuration(cursor, durationValue);
        draft.trimRange = draft.sourceRange;
        draft.name = clipObject.value(QStringLiteral("name")).toString();
        double speedScalar = 1.0;
        if (hasLinearTimeWarp(clipObject, speedScalar)) {
            draft.speed = speedScalar > 0.0 ? speedScalar : 1.0;
        }
        draft.reversed = clipMetadata.value(QStringLiteral("artifactReversed")).toBool(false);
        const ClipId importedClipId = store.addClip(
            store.track(trackId) ? store.track(trackId)->ownerSequenceId : SequenceId{},
            trackId, draft);
        if (!importedClipId.isValid()) {
            if (warnings) {
                warnings->push_back(
                    QStringLiteral("Failed to import clip: %1").arg(draft.name));
            }
        } else {
            importedAny = true;
        }
        if (importedClipId.isValid() && previousClipId.isValid() &&
            !pendingTransition.isEmpty()) {
            const qint64 inOffset = static_cast<qint64>(
                pendingTransition.value(QStringLiteral("in_offset")).toObject()
                    .value(QStringLiteral("value")).toDouble());
            const qint64 outOffset = static_cast<qint64>(
                pendingTransition.value(QStringLiteral("out_offset")).toObject()
                    .value(QStringLiteral("value")).toDouble());
            const double duration = static_cast<double>(inOffset + outOffset);
            const TransitionKind kind = parseTransitionKind(
                pendingTransition.value(QStringLiteral("transition_type")).toString(),
                pendingTransition.value(QStringLiteral("metadata")).toObject());
            store.createTransition(trackId, previousClipId, importedClipId,
                                   FrameRange::fromDuration(
                                       qMax<qint64>(0, cursor - inOffset),
                                       qMax<qint64>(1, static_cast<qint64>(duration))),
                                   kind, duration);
            pendingTransition = {};
        }
        if (importedClipId.isValid()) {
            previousClipId = importedClipId;
        }
        cursor += qMax<qint64>(0, durationValue);
    }
    return importedAny ? ImportChildResult::Imported : ImportChildResult::Skipped;
}

QJsonArray exportSequenceTracks(const NLEProjectStore& store,
                                const Sequence& sequence)
{
    const double rate = sequence.timeBase.fps();
    QJsonArray trackChildren;
    for (const TrackId& trackId : sequence.trackOrder) {
        const Track* track = store.track(trackId);
        if (!track) continue;

        QJsonArray children;
        qint64 cursor = 0;
        for (const ClipId& clipId : track->clipOrder) {
            const Clip* clip = store.clip(clipId);
            if (!clip) continue;
            const qint64 clipStart = clip->timelineRange.start();
            if (clipStart > cursor) {
                children.append(QJsonObject{
                    {QStringLiteral("OTIO_SCHEMA"), QStringLiteral("Gap.1")},
                    {QStringLiteral("name"), QStringLiteral("Gap")},
                    {QStringLiteral("duration"), rationalTime(clipStart - cursor, rate)}
                });
            }

            const SourceRef* source = store.source(clip->sourceId);
            QJsonObject metadata{
                {QStringLiteral("artifactClipId"), QString::number(clip->id.value)},
                {QStringLiteral("artifactTrackId"), QString::number(track->id.value)},
                {QStringLiteral("artifactTimelineRange"), QJsonObject{
                    {QStringLiteral("start"), clip->timelineRange.start()},
                    {QStringLiteral("duration"), clip->timelineRange.duration()}}},
                {QStringLiteral("artifactReversed"), clip->reversed}
            };

            QJsonObject clipObject{
                {QStringLiteral("OTIO_SCHEMA"), QStringLiteral("Clip.2")},
                {QStringLiteral("name"), clip->name},
                {QStringLiteral("source_range"), timeRange(clip->sourceRange, rate)},
                {QStringLiteral("metadata"), metadata}
            };
            if (clip->nestedSequenceId.isValid()) {
                const Sequence* nested = store.sequence(clip->nestedSequenceId);
                if (nested) {
                    metadata.insert(QStringLiteral("artifactNestedSequenceId"),
                                    QString::number(nested->id.value));
                    clipObject.insert(QStringLiteral("OTIO_SCHEMA"),
                                      QStringLiteral("Stack.1"));
                    clipObject.insert(QStringLiteral("name"), nested->name);
                    clipObject.insert(QStringLiteral("children"),
                                      exportSequenceTracks(store, *nested));
                    clipObject.remove(QStringLiteral("source_range"));
                } else {
                    QJsonObject mediaReference{
                        {QStringLiteral("OTIO_SCHEMA"), QStringLiteral("ExternalReference.1")},
                        {QStringLiteral("target_url"), source ? source->uri : QString()},
                        {QStringLiteral("name"), source ? source->displayName : QString()}};
                    clipObject.insert(QStringLiteral("media_reference"), mediaReference);
                }
            } else {
                QJsonObject mediaReference{
                    {QStringLiteral("OTIO_SCHEMA"), QStringLiteral("ExternalReference.1")},
                    {QStringLiteral("target_url"), source ? source->uri : QString()},
                    {QStringLiteral("name"), source ? source->displayName : QString()}};
                clipObject.insert(QStringLiteral("media_reference"), mediaReference);
                if (qAbs(clip->speed - 1.0) > 1e-9) {
                    clipObject.insert(QStringLiteral("effects"),
                                      QJsonArray{QJsonObject{
                                          {QStringLiteral("OTIO_SCHEMA"),
                                           QStringLiteral("LinearTimeWarp.1")},
                                          {QStringLiteral("name"), QStringLiteral("speed")},
                                          {QStringLiteral("time_scalar"), clip->speed}}});
                }
            }
            children.append(clipObject);
            for (const TransitionId& transitionId : track->transitions) {
                const Transition* transition = store.transition(transitionId);
                if (!transition || transition->leftClipId != clip->id) continue;
                const qint64 halfDuration = static_cast<qint64>(transition->duration / 2.0);
                children.append(QJsonObject{
                    {QStringLiteral("OTIO_SCHEMA"), QStringLiteral("Transition.1")},
                    {QStringLiteral("name"), transitionName(transition->kind)},
                    {QStringLiteral("transition_type"), transitionName(transition->kind)},
                    {QStringLiteral("in_offset"), rationalTime(halfDuration, rate)},
                    {QStringLiteral("out_offset"), rationalTime(transition->duration - halfDuration, rate)},
                    {QStringLiteral("metadata"), QJsonObject{
                        {QStringLiteral("artifactTransitionId"), QString::number(transition->id.value)},
                        {QStringLiteral("artifactKind"), static_cast<int>(transition->kind)}}}
                });
            }
            cursor = qMax(cursor, clipStart + clip->timelineRange.duration());
        }
        trackChildren.append(QJsonObject{
            {QStringLiteral("OTIO_SCHEMA"), QStringLiteral("Track.1")},
            {QStringLiteral("name"), track->name},
            {QStringLiteral("kind"), trackKind(track->kind)},
            {QStringLiteral("children"), children},
            {QStringLiteral("metadata"), QJsonObject{
                {QStringLiteral("artifactTrackId"), QString::number(track->id.value)},
                {QStringLiteral("enabled"), track->enabled},
                {QStringLiteral("locked"), track->locked}}}
        });
    }
    return trackChildren;
}

} // namespace

QJsonObject OtioAdapter::exportTimeline(const NLEProjectStore& store,
                                        const SequenceId& sequenceId)
{
    const Sequence* sequence = store.sequence(sequenceId);
    if (!sequence) {
        return {};
    }

    const double rate = sequence->timeBase.fps();

    QJsonArray markers;
    for (const MarkerId& markerId : sequence->markers) {
        const Marker* marker = store.marker(markerId);
        if (!marker) continue;
        markers.append(QJsonObject{
            {QStringLiteral("OTIO_SCHEMA"), QStringLiteral("Marker.2")},
            {QStringLiteral("name"), marker->name},
            {QStringLiteral("marked_range"), timeRange(FrameRange::fromDuration(marker->position.framePosition(), 1), rate)},
            {QStringLiteral("color"), markerColorName(marker->color)},
            {QStringLiteral("comment"), marker->note},
            {QStringLiteral("metadata"), QJsonObject{
                {QStringLiteral("artifactMarkerId"), QString::number(marker->id.value)},
                {QStringLiteral("artifactColor"), marker->color.name(QColor::HexArgb)}}}
        });
    }

    QJsonArray subtitleArray;
    for (const SubtitleCue& cue : sequence->subtitles) {
        subtitleArray.append(QJsonObject{
            {QStringLiteral("start"), cue.range.start()},
            {QStringLiteral("duration"), cue.range.duration()},
            {QStringLiteral("text"), cue.text},
            {QStringLiteral("name"), cue.name},
            {QStringLiteral("language"), cue.language},
            {QStringLiteral("speaker"), cue.speaker},
            {QStringLiteral("forced"), cue.forced},
            {QStringLiteral("closedCaption"), cue.closedCaption}});
    }

    return QJsonObject{
        {QStringLiteral("OTIO_SCHEMA"), QStringLiteral("Timeline.1")},
        {QStringLiteral("name"), sequence->name},
        {QStringLiteral("global_start_time"), rationalTime(sequence->duration.start(), rate)},
        {QStringLiteral("duration"), timeRange(sequence->duration, rate)},
        {QStringLiteral("tracks"), QJsonObject{
            {QStringLiteral("OTIO_SCHEMA"), QStringLiteral("Stack.1")},
            {QStringLiteral("children"), exportSequenceTracks(store, *sequence)}}},
        {QStringLiteral("markers"), markers},
        {QStringLiteral("metadata"), QJsonObject{
            {QStringLiteral("artifactSequenceId"), QString::number(sequence->id.value)},
            {QStringLiteral("artifactRateNumerator"), sequence->timeBase.numerator},
            {QStringLiteral("artifactRateDenominator"), sequence->timeBase.denominator},
            {QStringLiteral("artifactDropFrame"), sequence->timeBase.dropFrame},
            {QStringLiteral("artifactSubtitles"), subtitleArray}}}
    };
}

bool OtioAdapter::importTimeline(NLEProjectStore& store,
                                 const QJsonObject& timeline,
                                 SequenceId* importedSequenceId,
                                 QVector<QString>* warnings)
{
    if (timeline.value(QStringLiteral("OTIO_SCHEMA")).toString() != QStringLiteral("Timeline.1")) {
        if (warnings) warnings->push_back(QStringLiteral("Unsupported OTIO timeline schema"));
        return false;
    }

    const QJsonObject metadata = timeline.value(QStringLiteral("metadata")).toObject();
    TimeBase timeBase;
    timeBase.numerator = metadata.value(QStringLiteral("artifactRateNumerator")).toInt(1);
    timeBase.denominator = metadata.value(QStringLiteral("artifactRateDenominator")).toInt(30);
    timeBase.dropFrame = metadata.value(QStringLiteral("artifactDropFrame")).toBool(false);

    const SequenceId sequenceId = importSequenceTracks(
        store, timeline.value(QStringLiteral("name")).toString(), timeBase,
        timeline.value(QStringLiteral("tracks")).toObject()
            .value(QStringLiteral("children")).toArray(),
        warnings);
    if (importedSequenceId) {
        *importedSequenceId = sequenceId;
    }
    if (!sequenceId.isValid()) {
        if (warnings) warnings->push_back(QStringLiteral("Failed to import OTIO timeline"));
        return false;
    }
    Sequence* importedSequence = store.sequence(sequenceId);
    if (!importedSequence) {
        return false;
    }

    // Subtitles live under metadata for standards-safe interchange; the
    // legacy top-level key is still accepted for older exports.
    QJsonValue subtitleValue = metadata.value(QStringLiteral("artifactSubtitles"));
    if (!subtitleValue.isArray()) {
        subtitleValue = timeline.value(QStringLiteral("subtitles"));
    }
    for (const QJsonValue& entry : subtitleValue.toArray()) {
        const QJsonObject subtitleObject = entry.toObject();
        SubtitleCue cue;
        cue.range = FrameRange::fromDuration(
            subtitleObject.value(QStringLiteral("start")).toVariant().toLongLong(),
            subtitleObject.value(QStringLiteral("duration")).toVariant().toLongLong());
        cue.text = subtitleObject.value(QStringLiteral("text")).toString();
        cue.name = subtitleObject.value(QStringLiteral("name")).toString();
        cue.language = subtitleObject.value(QStringLiteral("language")).toString();
        cue.speaker = subtitleObject.value(QStringLiteral("speaker")).toString();
        cue.forced = subtitleObject.value(QStringLiteral("forced")).toBool(false);
        cue.closedCaption = subtitleObject.value(QStringLiteral("closedCaption")).toBool(false);
        if (cue.range.duration() > 0 && !cue.text.trimmed().isEmpty()) {
            importedSequence->subtitles.push_back(std::move(cue));
        }
    }

    for (const QJsonValue& markerValue : timeline.value(QStringLiteral("markers")).toArray()) {
        const QJsonObject markerObject = markerValue.toObject();
        const QJsonObject markedRange = markerObject.value(QStringLiteral("marked_range")).toObject();
        const qint64 position = static_cast<qint64>(markedRange.value(QStringLiteral("start_time")).toObject()
            .value(QStringLiteral("value")).toDouble());
        store.createMarker(sequenceId,
                           FramePosition(position),
                           markerObject.value(QStringLiteral("name")).toString(),
                           markerObject.value(QStringLiteral("comment")).toString(),
                           parseMarkerColor(markerObject));
    }

    return true;
}

QVector<SubtitleCue> OtioAdapter::importSrt(const QString& text,
                                            const TimeBase& timeBase,
                                            QVector<QString>* warnings)
{
    QVector<SubtitleCue> result;
    const QString normalized = text.toUtf8().replace("\r\n", "\n").replace('\r', '\n');
    const QStringList lines = normalized.split(QChar('\n'));
    const QRegularExpression timing(
        QStringLiteral(R"(^\s*(\d{2}):(\d{2}):(\d{2}),(\d{3})\s*-->\s*(\d{2}):(\d{2}):(\d{2}),(\d{3})\s*$)"));
    auto parseTime = [&timeBase](const QRegularExpressionMatch& match, int offset) -> qint64 {
        const qint64 h = match.captured(offset).toLongLong();
        const qint64 m = match.captured(offset + 1).toLongLong();
        const qint64 s = match.captured(offset + 2).toLongLong();
        const qint64 ms = match.captured(offset + 3).toLongLong();
        const double frame = (static_cast<double>(h * 3600 + m * 60 + s) + ms / 1000.0) * timeBase.fps();
        return static_cast<qint64>(std::llround(frame));
    };
    int line = 0;
    while (line < lines.size()) {
        while (line < lines.size() && lines[line].trimmed().isEmpty()) ++line;
        if (line >= lines.size()) break;
        ++line; // cue number (kept permissive for common SRT variants)
        if (line >= lines.size()) break;
        const auto match = timing.match(lines[line++]);
        if (!match.hasMatch()) {
            if (warnings) warnings->push_back(QStringLiteral("Invalid SRT timing near line %1").arg(line));
            while (line < lines.size() && !lines[line].trimmed().isEmpty()) ++line;
            continue;
        }
        QStringList payload;
        while (line < lines.size() && !lines[line].trimmed().isEmpty()) payload.push_back(lines[line++]);
        const qint64 start = parseTime(match, 1);
        const qint64 end = parseTime(match, 5);
        if (end <= start) {
            if (warnings) warnings->push_back(QStringLiteral("SRT cue has non-positive duration near line %1").arg(line));
            continue;
        }
        SubtitleCue cue;
        cue.range = FrameRange::fromDuration(start, end - start);
        cue.text = payload.join(QChar('\n')).trimmed();
        cue.name = cue.text.section(QChar('\n'), 0, 0).left(64);
        result.push_back(std::move(cue));
    }
    std::sort(result.begin(), result.end(), [](const SubtitleCue& a, const SubtitleCue& b) {
        if (a.range.start() != b.range.start()) return a.range.start() < b.range.start();
        return a.range.duration() < b.range.duration();
    });
    return result;
}

QVector<SubtitleCue> OtioAdapter::importWebVtt(const QString& text,
                                                const TimeBase& timeBase,
                                                QVector<QString>* warnings)
{
    const QString normalized = text.toUtf8().replace("\r\n", "\n").replace('\r', '\n');
    const QStringList lines = normalized.split(QChar('\n'));
    QString srt;
    int cueNumber = 1;
    int line = 0;
    while (line < lines.size()) {
        while (line < lines.size() && lines[line].trimmed().isEmpty()) ++line;
        if (line >= lines.size()) break;
        const QString header = lines[line].trimmed();
        if (header.compare(QStringLiteral("WEBVTT"), Qt::CaseInsensitive) == 0 ||
            header.startsWith(QStringLiteral("NOTE"), Qt::CaseInsensitive) ||
            header.startsWith(QStringLiteral("STYLE"), Qt::CaseInsensitive) ||
            header.startsWith(QStringLiteral("REGION"), Qt::CaseInsensitive)) {
            while (line < lines.size() && !lines[line].trimmed().isEmpty()) ++line;
            continue;
        }
        if (!lines[line].contains(QStringLiteral("-->"))) {
            ++line;
            if (line >= lines.size()) break;
        }
        QString timing = lines[line++].trimmed();
        if (!timing.contains(QStringLiteral("-->"))) {
            if (warnings) warnings->push_back(QStringLiteral("Invalid WebVTT timing near line %1").arg(line));
            while (line < lines.size() && !lines[line].trimmed().isEmpty()) ++line;
            continue;
        }
        timing.replace(QRegularExpression(QStringLiteral("(\\d{2}:\\d{2}:\\d{2})\\.(\\d{3})")), QStringLiteral("\\1,\\2"));
        srt += QString::number(cueNumber++) + QChar('\n') + timing + QChar('\n');
        while (line < lines.size() && !lines[line].trimmed().isEmpty()) {
            srt += lines[line++] + QChar('\n');
        }
        srt += QChar('\n');
    }
    return importSrt(srt, timeBase, warnings);
}

QString OtioAdapter::exportSrt(const QVector<SubtitleCue>& cues,
                               const TimeBase& timeBase)
{
    auto formatTime = [&timeBase](qint64 frame) {
        const qint64 totalMs = static_cast<qint64>(std::llround(
            static_cast<double>(frame) * 1000.0 / timeBase.fps()));
        const qint64 hours = totalMs / 3600000;
        const qint64 minutes = (totalMs / 60000) % 60;
        const qint64 seconds = (totalMs / 1000) % 60;
        const qint64 millis = totalMs % 1000;
        return QStringLiteral("%1:%2:%3,%4").arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'))
            .arg(millis, 3, 10, QChar('0'));
    };
    QString output;
    QVector<SubtitleCue> ordered = cues;
    std::sort(ordered.begin(), ordered.end(), [](const SubtitleCue& a, const SubtitleCue& b) {
        if (a.range.start() != b.range.start()) return a.range.start() < b.range.start();
        return a.range.duration() < b.range.duration();
    });
    int index = 1;
    for (const SubtitleCue& cue : ordered) {
        if (cue.range.duration() <= 0 || cue.text.trimmed().isEmpty()) continue;
        output += QString::number(index++) + QStringLiteral("\n") +
                  formatTime(cue.range.start()) + QStringLiteral(" --> ") +
                  formatTime(cue.range.start() + cue.range.duration()) + QStringLiteral("\n") +
                  cue.text.trimmed() + QStringLiteral("\n\n");
    }
    return output;
}

QString OtioAdapter::exportWebVtt(const QVector<SubtitleCue>& cues,
                                  const TimeBase& timeBase)
{
    QString output = QStringLiteral("WEBVTT\n\n");
    const QString srt = exportSrt(cues, timeBase);
    const QStringList lines = srt.split(QChar('\n'));
    for (const QString& line : lines) {
        if (line.contains(QStringLiteral("-->"))) {
            QString converted = line;
            converted.replace(QRegularExpression(QStringLiteral("(\\d{2}:\\d{2}:\\d{2}),(\\d{3})")), QStringLiteral("\\1.\\2"));
            output += converted + QChar('\n');
        } else if (!line.trimmed().isEmpty() && line.trimmed().toInt() > 0) {
            continue;
        } else {
            output += line + QChar('\n');
        }
    }
    return output;
}

bool OtioAdapter::importSrtIntoSequence(NLEProjectStore& store,
                                        const SequenceId& sequenceId,
                                        const QString& text,
                                        QVector<QString>* warnings)
{
    Sequence* sequence = store.sequence(sequenceId);
    if (!sequence) {
        if (warnings) warnings->push_back(QStringLiteral("Subtitle import target sequence not found"));
        return false;
    }
    const QVector<SubtitleCue> imported = importSrt(text, sequence->timeBase, warnings);
    sequence->subtitles = imported;
    return true;
}

QString OtioAdapter::exportSrtFromSequence(const NLEProjectStore& store,
                                           const SequenceId& sequenceId)
{
    const Sequence* sequence = store.sequence(sequenceId);
    return sequence ? exportSrt(sequence->subtitles, sequence->timeBase) : QString();
}

bool OtioAdapter::importSrtFileIntoSequence(NLEProjectStore& store,
                                            const SequenceId& sequenceId,
                                            const QString& filePath,
                                            QVector<QString>* warnings)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (warnings) warnings->push_back(QStringLiteral("Could not open SRT file: %1").arg(filePath));
        return false;
    }
    return importSrtIntoSequence(store, sequenceId, QString::fromUtf8(file.readAll()), warnings);
}

bool OtioAdapter::exportSrtFile(const NLEProjectStore& store,
                                const SequenceId& sequenceId,
                                const QString& filePath)
{
    const QString text = exportSrtFromSequence(store, sequenceId);
    if (text.isNull()) return false;
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    const QByteArray encoded = text.toUtf8();
    if (file.write(encoded) != encoded.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

} // namespace ArtifactCore::NLE
