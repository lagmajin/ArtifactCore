module;

#include <QJsonArray>
#include <QJsonObject>
#include <QColor>
#include <QString>
#include <QVector>

module NLE.OTIO;

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
    return TrackKind::Video;
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
    QJsonArray trackChildren;
    for (const TrackId& trackId : sequence->trackOrder) {
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
                    {QStringLiteral("duration"), clip->timelineRange.duration()}}}
            };
            QJsonObject mediaReference{
                {QStringLiteral("OTIO_SCHEMA"), QStringLiteral("ExternalReference.1")},
                {QStringLiteral("target_url"), source ? source->uri : QString()},
                {QStringLiteral("name"), source ? source->displayName : QString()}
            };
            children.append(QJsonObject{
                {QStringLiteral("OTIO_SCHEMA"), QStringLiteral("Clip.2")},
                {QStringLiteral("name"), clip->name},
                {QStringLiteral("source_range"), timeRange(clip->sourceRange, rate)},
                {QStringLiteral("media_reference"), mediaReference},
                {QStringLiteral("metadata"), metadata}
            });
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

    QJsonArray markers;
    for (const MarkerId& markerId : sequence->markers) {
        const Marker* marker = store.marker(markerId);
        if (!marker) continue;
        markers.append(QJsonObject{
            {QStringLiteral("OTIO_SCHEMA"), QStringLiteral("Marker.2")},
            {QStringLiteral("name"), marker->name},
            {QStringLiteral("marked_range"), timeRange(FrameRange::fromDuration(marker->position.framePosition(), 1), rate)},
            {QStringLiteral("color"), marker->color.name(QColor::HexArgb)},
            {QStringLiteral("comment"), marker->note},
            {QStringLiteral("metadata"), QJsonObject{
                {QStringLiteral("artifactMarkerId"), QString::number(marker->id.value)}}}
        });
    }

    return QJsonObject{
        {QStringLiteral("OTIO_SCHEMA"), QStringLiteral("Timeline.1")},
        {QStringLiteral("name"), sequence->name},
        {QStringLiteral("global_start_time"), rationalTime(sequence->duration.start(), rate)},
        {QStringLiteral("duration"), timeRange(sequence->duration, rate)},
        {QStringLiteral("tracks"), QJsonObject{
            {QStringLiteral("OTIO_SCHEMA"), QStringLiteral("Stack.1")},
            {QStringLiteral("children"), trackChildren}}},
        {QStringLiteral("markers"), markers},
        {QStringLiteral("metadata"), QJsonObject{
            {QStringLiteral("artifactSequenceId"), QString::number(sequence->id.value)},
            {QStringLiteral("artifactRateNumerator"), sequence->timeBase.numerator},
            {QStringLiteral("artifactRateDenominator"), sequence->timeBase.denominator},
            {QStringLiteral("artifactDropFrame"), sequence->timeBase.dropFrame}}}
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
    const SequenceId sequenceId = store.createSequence(timeline.value(QStringLiteral("name")).toString(), timeBase);
    if (importedSequenceId) *importedSequenceId = sequenceId;

    for (const QJsonValue& markerValue : timeline.value(QStringLiteral("markers")).toArray()) {
        const QJsonObject markerObject = markerValue.toObject();
        const QJsonObject markedRange = markerObject.value(QStringLiteral("marked_range")).toObject();
        const qint64 position = static_cast<qint64>(markedRange.value(QStringLiteral("start_time")).toObject()
            .value(QStringLiteral("value")).toDouble());
        QColor color(markerObject.value(QStringLiteral("color")).toString());
        if (!color.isValid()) color = QColor(Qt::yellow);
        store.createMarker(sequenceId,
                           FramePosition(position),
                           markerObject.value(QStringLiteral("name")).toString(),
                           markerObject.value(QStringLiteral("comment")).toString(),
                           color);
    }

    const QJsonArray tracks = timeline.value(QStringLiteral("tracks")).toObject().value(QStringLiteral("children")).toArray();
    for (const QJsonValue& trackValue : tracks) {
        const QJsonObject trackObject = trackValue.toObject();
        const TrackId trackId = store.createTrack(sequenceId,
                                                  parseTrackKind(trackObject.value(QStringLiteral("kind")).toString()),
                                                  trackObject.value(QStringLiteral("name")).toString());
        const QJsonArray clips = trackObject.value(QStringLiteral("children")).toArray();
        qint64 cursor = 0;
        for (const QJsonValue& clipValue : clips) {
            const QJsonObject clipObject = clipValue.toObject();
            const QString schema = clipObject.value(QStringLiteral("OTIO_SCHEMA")).toString();
            if (schema.startsWith(QStringLiteral("Gap."))) {
                const qint64 gapDuration = static_cast<qint64>(clipObject.value(QStringLiteral("duration")).toObject()
                    .value(QStringLiteral("value")).toDouble());
                cursor += qMax<qint64>(0, gapDuration);
                continue;
            }
            if (!schema.startsWith(QStringLiteral("Clip."))) {
                if (warnings) warnings->push_back(QStringLiteral("Skipped unsupported OTIO child in track"));
                continue;
            }
            const QJsonObject sourceRangeObject = clipObject.value(QStringLiteral("source_range")).toObject();
            const QJsonObject sourceStart = sourceRangeObject.value(QStringLiteral("start_time")).toObject();
            const QJsonObject sourceDuration = sourceRangeObject.value(QStringLiteral("duration")).toObject();
            const qint64 sourceStartValue = static_cast<qint64>(sourceStart.value(QStringLiteral("value")).toDouble());
            const qint64 durationValue = static_cast<qint64>(sourceDuration.value(QStringLiteral("value")).toDouble());
            const QJsonObject media = clipObject.value(QStringLiteral("media_reference")).toObject();
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
            if (!store.addClip(sequenceId, trackId, draft) && warnings) {
                warnings->push_back(QStringLiteral("Failed to import clip: %1").arg(draft.name));
            }
            cursor += qMax<qint64>(0, durationValue);
        }
    }
    return true;
}

} // namespace ArtifactCore::NLE
