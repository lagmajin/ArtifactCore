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
    return TrackKind::Video;
}

QString transitionName(const TransitionKind kind)
{
    switch (kind) {
    case TransitionKind::Cut: return QStringLiteral("Cut");
    case TransitionKind::Dissolve: return QStringLiteral("Dissolve");
    default: return QStringLiteral("Crossfade");
    }
}

TransitionKind parseTransitionKind(const QString& value)
{
    if (value.compare(QStringLiteral("Cut"), Qt::CaseInsensitive) == 0) return TransitionKind::Cut;
    if (value.compare(QStringLiteral("Dissolve"), Qt::CaseInsensitive) == 0) return TransitionKind::Dissolve;
    return TransitionKind::Crossfade;
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
    QJsonArray subtitles;
    for (const SubtitleCue& cue : sequence->subtitles) {
        subtitles.append(QJsonObject{
            {QStringLiteral("start"), cue.range.start()},
            {QStringLiteral("duration"), cue.range.duration()},
            {QStringLiteral("text"), cue.text},
            {QStringLiteral("name"), cue.name}});
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
        {QStringLiteral("subtitles"), subtitles},
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
    if (Sequence* importedSequence = store.sequence(sequenceId)) {
        for (const QJsonValue& subtitleValue : timeline.value(QStringLiteral("subtitles")).toArray()) {
            const QJsonObject subtitleObject = subtitleValue.toObject();
            SubtitleCue cue;
            cue.range = FrameRange::fromDuration(
                subtitleObject.value(QStringLiteral("start")).toVariant().toLongLong(),
                subtitleObject.value(QStringLiteral("duration")).toVariant().toLongLong());
            cue.text = subtitleObject.value(QStringLiteral("text")).toString();
            cue.name = subtitleObject.value(QStringLiteral("name")).toString();
            if (cue.range.duration() > 0 && !cue.text.trimmed().isEmpty()) {
                importedSequence->subtitles.push_back(std::move(cue));
            }
        }
    }

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
        ClipId previousClipId;
        QJsonObject pendingTransition;
        for (const QJsonValue& clipValue : clips) {
            const QJsonObject clipObject = clipValue.toObject();
            const QString schema = clipObject.value(QStringLiteral("OTIO_SCHEMA")).toString();
            if (schema.startsWith(QStringLiteral("Gap."))) {
                const qint64 gapDuration = static_cast<qint64>(clipObject.value(QStringLiteral("duration")).toObject()
                    .value(QStringLiteral("value")).toDouble());
                cursor += qMax<qint64>(0, gapDuration);
                continue;
            }
            if (schema.startsWith(QStringLiteral("Transition."))) {
                pendingTransition = clipObject;
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
            const ClipId importedClipId = store.addClip(sequenceId, trackId, draft);
            if (!importedClipId.isValid() && warnings) {
                warnings->push_back(QStringLiteral("Failed to import clip: %1").arg(draft.name));
            }
            if (importedClipId.isValid() && previousClipId.isValid() && !pendingTransition.isEmpty()) {
                const qint64 inOffset = static_cast<qint64>(pendingTransition.value(QStringLiteral("in_offset"))
                    .toObject().value(QStringLiteral("value")).toDouble());
                const qint64 outOffset = static_cast<qint64>(pendingTransition.value(QStringLiteral("out_offset"))
                    .toObject().value(QStringLiteral("value")).toDouble());
                const double duration = static_cast<double>(inOffset + outOffset);
                const TransitionKind kind = parseTransitionKind(
                    pendingTransition.value(QStringLiteral("transition_type")).toString());
                store.createTransition(trackId, previousClipId, importedClipId,
                                       FrameRange::fromDuration(qMax<qint64>(0, cursor - inOffset),
                                                                qMax<qint64>(1, static_cast<qint64>(duration))),
                                       kind, duration);
                pendingTransition = {};
            }
            if (importedClipId.isValid()) previousClipId = importedClipId;
            cursor += qMax<qint64>(0, durationValue);
        }
    }
    return true;
}

QVector<SubtitleCue> OtioAdapter::importSrt(const QString& text,
                                            const TimeBase& timeBase,
                                            QVector<QString>* warnings)
{
    QVector<SubtitleCue> result;
    const QString normalized = text.toUtf8().replace("\r\n", "\n").replace('\r', '\n');
    const QStringList lines = QString::fromUtf8(normalized).split(QChar('\n'));
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
