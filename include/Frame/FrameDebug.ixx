module;

#include <cstdint>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <vector>

export module Frame.Debug;

import Frame.Position;
import Frame.Range;

export namespace ArtifactCore {

enum class FrameDebugPassKind {
    Unknown,
    Clear,
    Draw,
    Resolve,
    Readback,
    Upload,
    Composite,
    Encode
};

enum class FrameDebugPassStatus {
    Unknown,
    Pending,
    Success,
    Skipped,
    Failed
};

enum class FrameDebugCompareMode {
    Disabled,
    Previous,
    Next,
    CaptureId
};

struct RenderCostStats {
    std::uint64_t drawCalls = 0;
    std::uint64_t indexedDrawCalls = 0;
    std::uint64_t psoSwitches = 0;
    std::uint64_t srbCommits = 0;
    std::uint64_t bufferUpdates = 0;
};

struct FrameDebugTextureRef {
    QString name;
    QString format;
    int width = 0;
    int height = 0;
    int mipLevel = 0;
    int mipLevels = 1;
    int sliceIndex = 0;
    int arrayLayers = 1;
    int sampleCount = 1;
    bool srgb = false;
    bool valid = false;

    [[nodiscard]] QJsonObject toJson() const;
    static FrameDebugTextureRef fromJson(const QJsonObject& json);
};

struct FrameDebugBufferRef {
    QString name;
    std::uint64_t byteSize = 0;
    bool valid = false;

    [[nodiscard]] QJsonObject toJson() const;
    static FrameDebugBufferRef fromJson(const QJsonObject& json);
};

struct FrameDebugAttachmentRecord {
    QString name;
    QString role;
    FrameDebugTextureRef texture;
    FrameDebugBufferRef buffer;
    bool readOnly = true;

    [[nodiscard]] QJsonObject toJson() const;
    static FrameDebugAttachmentRecord fromJson(const QJsonObject& json);
};

struct FrameDebugResourceRecord {
    QString label;
    QString type;
    QString relation;
    QString note;
    FrameDebugTextureRef texture;
    FrameDebugBufferRef buffer;
    bool cacheHit = false;
    bool stale = false;

    [[nodiscard]] QJsonObject toJson() const;
    static FrameDebugResourceRecord fromJson(const QJsonObject& json);
};

struct FrameDebugPassRecord {
    QString name;
    FrameDebugPassKind kind = FrameDebugPassKind::Unknown;
    FrameDebugPassStatus status = FrameDebugPassStatus::Unknown;
    std::int64_t durationUs = 0;
    std::vector<FrameDebugAttachmentRecord> inputs;
    std::vector<FrameDebugAttachmentRecord> outputs;
    QString note;

    [[nodiscard]] QJsonObject toJson() const;
    static FrameDebugPassRecord fromJson(const QJsonObject& json);
};

struct FrameDebugSnapshot {
    FramePosition frame;
    std::int64_t timestampMs = 0;
    QString compositionName;
    QString renderBackend;
    QString playbackState;
    QString selectedLayerName;
    double renderLastFrameMs = 0.0;
    double renderAverageFrameMs = 0.0;
    double renderGpuFrameMs = 0.0;
    RenderCostStats renderCost;
    FrameDebugCompareMode compareMode = FrameDebugCompareMode::Disabled;
    QString compareTargetId;
    std::vector<FrameDebugPassRecord> passes;
    std::vector<FrameDebugResourceRecord> resources;
    std::vector<FrameDebugAttachmentRecord> attachments;
    bool failed = false;
    QString failureReason;

    [[nodiscard]] QJsonObject toJson() const;
    static FrameDebugSnapshot fromJson(const QJsonObject& json);
};

struct FrameDebugCapture {
    QString captureId;
    FrameDebugSnapshot snapshot;
    QString sourceFrameId;
    QString targetFrameId;
    bool pinned = false;

    [[nodiscard]] QJsonObject toJson() const;
    static FrameDebugCapture fromJson(const QJsonObject& json);
};

struct FrameDebugBundle {
    QString bundleId;
    QString label;
    std::int64_t createdAtMs = 0;
    FrameDebugCapture capture;
    std::vector<FrameDebugCapture> history;

    [[nodiscard]] QJsonObject toJson() const;
    static FrameDebugBundle fromJson(const QJsonObject& json);
};

inline QString toString(FrameDebugPassKind kind)
{
    switch (kind) {
    case FrameDebugPassKind::Clear: return QStringLiteral("Clear");
    case FrameDebugPassKind::Draw: return QStringLiteral("Draw");
    case FrameDebugPassKind::Resolve: return QStringLiteral("Resolve");
    case FrameDebugPassKind::Readback: return QStringLiteral("Readback");
    case FrameDebugPassKind::Upload: return QStringLiteral("Upload");
    case FrameDebugPassKind::Composite: return QStringLiteral("Composite");
    case FrameDebugPassKind::Encode: return QStringLiteral("Encode");
    default: return QStringLiteral("Unknown");
    }
}

inline QString toString(FrameDebugPassStatus status)
{
    switch (status) {
    case FrameDebugPassStatus::Pending: return QStringLiteral("Pending");
    case FrameDebugPassStatus::Success: return QStringLiteral("Success");
    case FrameDebugPassStatus::Skipped: return QStringLiteral("Skipped");
    case FrameDebugPassStatus::Failed: return QStringLiteral("Failed");
    default: return QStringLiteral("Unknown");
    }
}

inline QString toString(FrameDebugCompareMode mode)
{
    switch (mode) {
    case FrameDebugCompareMode::Previous: return QStringLiteral("Previous");
    case FrameDebugCompareMode::Next: return QStringLiteral("Next");
    case FrameDebugCompareMode::CaptureId: return QStringLiteral("CaptureId");
    default: return QStringLiteral("Disabled");
    }
}

inline FrameDebugPassKind passKindFromString(const QString& value)
{
    if (value == QStringLiteral("Clear")) return FrameDebugPassKind::Clear;
    if (value == QStringLiteral("Draw")) return FrameDebugPassKind::Draw;
    if (value == QStringLiteral("Resolve")) return FrameDebugPassKind::Resolve;
    if (value == QStringLiteral("Readback")) return FrameDebugPassKind::Readback;
    if (value == QStringLiteral("Upload")) return FrameDebugPassKind::Upload;
    if (value == QStringLiteral("Composite")) return FrameDebugPassKind::Composite;
    if (value == QStringLiteral("Encode")) return FrameDebugPassKind::Encode;
    return FrameDebugPassKind::Unknown;
}

inline FrameDebugPassStatus passStatusFromString(const QString& value)
{
    if (value == QStringLiteral("Pending")) return FrameDebugPassStatus::Pending;
    if (value == QStringLiteral("Success")) return FrameDebugPassStatus::Success;
    if (value == QStringLiteral("Skipped")) return FrameDebugPassStatus::Skipped;
    if (value == QStringLiteral("Failed")) return FrameDebugPassStatus::Failed;
    return FrameDebugPassStatus::Unknown;
}

inline FrameDebugCompareMode compareModeFromString(const QString& value)
{
    if (value == QStringLiteral("Previous")) return FrameDebugCompareMode::Previous;
    if (value == QStringLiteral("Next")) return FrameDebugCompareMode::Next;
    if (value == QStringLiteral("CaptureId")) return FrameDebugCompareMode::CaptureId;
    return FrameDebugCompareMode::Disabled;
}

inline QJsonObject FrameDebugTextureRef::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("name"), name);
    json.insert(QStringLiteral("format"), format);
    json.insert(QStringLiteral("width"), width);
    json.insert(QStringLiteral("height"), height);
    json.insert(QStringLiteral("mipLevel"), mipLevel);
    json.insert(QStringLiteral("mipLevels"), mipLevels);
    json.insert(QStringLiteral("sliceIndex"), sliceIndex);
    json.insert(QStringLiteral("arrayLayers"), arrayLayers);
    json.insert(QStringLiteral("sampleCount"), sampleCount);
    json.insert(QStringLiteral("srgb"), srgb);
    json.insert(QStringLiteral("valid"), valid);
    return json;
}

inline FrameDebugTextureRef FrameDebugTextureRef::fromJson(const QJsonObject& json)
{
    FrameDebugTextureRef ref;
    ref.name = json.value(QStringLiteral("name")).toString();
    ref.format = json.value(QStringLiteral("format")).toString();
    ref.width = json.value(QStringLiteral("width")).toInt();
    ref.height = json.value(QStringLiteral("height")).toInt();
    ref.mipLevel = json.value(QStringLiteral("mipLevel")).toInt();
    ref.mipLevels = json.value(QStringLiteral("mipLevels")).toInt(1);
    ref.sliceIndex = json.value(QStringLiteral("sliceIndex")).toInt();
    ref.arrayLayers = json.value(QStringLiteral("arrayLayers")).toInt(1);
    ref.sampleCount = json.value(QStringLiteral("sampleCount")).toInt(1);
    ref.srgb = json.value(QStringLiteral("srgb")).toBool();
    ref.valid = json.value(QStringLiteral("valid")).toBool();
    return ref;
}

inline QJsonObject FrameDebugBufferRef::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("name"), name);
    json.insert(QStringLiteral("byteSize"), static_cast<double>(byteSize));
    json.insert(QStringLiteral("valid"), valid);
    return json;
}

inline FrameDebugBufferRef FrameDebugBufferRef::fromJson(const QJsonObject& json)
{
    FrameDebugBufferRef ref;
    ref.name = json.value(QStringLiteral("name")).toString();
    ref.byteSize = static_cast<std::uint64_t>(json.value(QStringLiteral("byteSize")).toDouble());
    ref.valid = json.value(QStringLiteral("valid")).toBool();
    return ref;
}

inline QJsonObject FrameDebugAttachmentRecord::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("name"), name);
    json.insert(QStringLiteral("role"), role);
    json.insert(QStringLiteral("texture"), texture.toJson());
    json.insert(QStringLiteral("buffer"), buffer.toJson());
    json.insert(QStringLiteral("readOnly"), readOnly);
    return json;
}

inline FrameDebugAttachmentRecord FrameDebugAttachmentRecord::fromJson(const QJsonObject& json)
{
    FrameDebugAttachmentRecord record;
    record.name = json.value(QStringLiteral("name")).toString();
    record.role = json.value(QStringLiteral("role")).toString();
    record.texture = FrameDebugTextureRef::fromJson(json.value(QStringLiteral("texture")).toObject());
    record.buffer = FrameDebugBufferRef::fromJson(json.value(QStringLiteral("buffer")).toObject());
    record.readOnly = json.value(QStringLiteral("readOnly")).toBool(true);
    return record;
}

inline QJsonObject FrameDebugResourceRecord::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("label"), label);
    json.insert(QStringLiteral("type"), type);
    json.insert(QStringLiteral("relation"), relation);
    json.insert(QStringLiteral("note"), note);
    json.insert(QStringLiteral("texture"), texture.toJson());
    json.insert(QStringLiteral("buffer"), buffer.toJson());
    json.insert(QStringLiteral("cacheHit"), cacheHit);
    json.insert(QStringLiteral("stale"), stale);
    return json;
}

inline FrameDebugResourceRecord FrameDebugResourceRecord::fromJson(const QJsonObject& json)
{
    FrameDebugResourceRecord record;
    record.label = json.value(QStringLiteral("label")).toString();
    record.type = json.value(QStringLiteral("type")).toString();
    record.relation = json.value(QStringLiteral("relation")).toString();
    record.note = json.value(QStringLiteral("note")).toString();
    record.texture = FrameDebugTextureRef::fromJson(json.value(QStringLiteral("texture")).toObject());
    record.buffer = FrameDebugBufferRef::fromJson(json.value(QStringLiteral("buffer")).toObject());
    record.cacheHit = json.value(QStringLiteral("cacheHit")).toBool();
    record.stale = json.value(QStringLiteral("stale")).toBool();
    return record;
}

inline QJsonObject FrameDebugPassRecord::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("name"), name);
    json.insert(QStringLiteral("kind"), toString(kind));
    json.insert(QStringLiteral("status"), toString(status));
    json.insert(QStringLiteral("durationUs"), static_cast<double>(durationUs));
    json.insert(QStringLiteral("note"), note);

    QJsonArray inputsJson;
    for (const auto& input : inputs) {
        inputsJson.append(input.toJson());
    }
    json.insert(QStringLiteral("inputs"), inputsJson);

    QJsonArray outputsJson;
    for (const auto& output : outputs) {
        outputsJson.append(output.toJson());
    }
    json.insert(QStringLiteral("outputs"), outputsJson);
    return json;
}

inline FrameDebugPassRecord FrameDebugPassRecord::fromJson(const QJsonObject& json)
{
    FrameDebugPassRecord record;
    record.name = json.value(QStringLiteral("name")).toString();
    record.kind = passKindFromString(json.value(QStringLiteral("kind")).toString());
    record.status = passStatusFromString(json.value(QStringLiteral("status")).toString());
    record.durationUs = static_cast<std::int64_t>(json.value(QStringLiteral("durationUs")).toDouble());
    record.note = json.value(QStringLiteral("note")).toString();

    for (const auto& value : json.value(QStringLiteral("inputs")).toArray()) {
        record.inputs.push_back(FrameDebugAttachmentRecord::fromJson(value.toObject()));
    }
    for (const auto& value : json.value(QStringLiteral("outputs")).toArray()) {
        record.outputs.push_back(FrameDebugAttachmentRecord::fromJson(value.toObject()));
    }
    return record;
}

inline QJsonObject FrameDebugSnapshot::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("frame"), static_cast<double>(frame.framePosition()));
    json.insert(QStringLiteral("timestampMs"), static_cast<double>(timestampMs));
    json.insert(QStringLiteral("compositionName"), compositionName);
    json.insert(QStringLiteral("renderBackend"), renderBackend);
    json.insert(QStringLiteral("playbackState"), playbackState);
    json.insert(QStringLiteral("selectedLayerName"), selectedLayerName);
    json.insert(QStringLiteral("renderLastFrameMs"), renderLastFrameMs);
    json.insert(QStringLiteral("renderAverageFrameMs"), renderAverageFrameMs);
    json.insert(QStringLiteral("renderGpuFrameMs"), renderGpuFrameMs);
    json.insert(QStringLiteral("renderDrawCalls"), static_cast<double>(renderCost.drawCalls));
    json.insert(QStringLiteral("renderIndexedDrawCalls"), static_cast<double>(renderCost.indexedDrawCalls));
    json.insert(QStringLiteral("renderPsoSwitches"), static_cast<double>(renderCost.psoSwitches));
    json.insert(QStringLiteral("renderSrbCommits"), static_cast<double>(renderCost.srbCommits));
    json.insert(QStringLiteral("renderBufferUpdates"), static_cast<double>(renderCost.bufferUpdates));
    json.insert(QStringLiteral("compareMode"), toString(compareMode));
    json.insert(QStringLiteral("compareTargetId"), compareTargetId);
    json.insert(QStringLiteral("failed"), failed);
    json.insert(QStringLiteral("failureReason"), failureReason);

    QJsonArray passesJson;
    for (const auto& pass : passes) {
        passesJson.append(pass.toJson());
    }
    json.insert(QStringLiteral("passes"), passesJson);

    QJsonArray resourcesJson;
    for (const auto& resource : resources) {
        resourcesJson.append(resource.toJson());
    }
    json.insert(QStringLiteral("resources"), resourcesJson);

    QJsonArray attachmentsJson;
    for (const auto& attachment : attachments) {
        attachmentsJson.append(attachment.toJson());
    }
    json.insert(QStringLiteral("attachments"), attachmentsJson);
    return json;
}

inline FrameDebugSnapshot FrameDebugSnapshot::fromJson(const QJsonObject& json)
{
    FrameDebugSnapshot snapshot;
    snapshot.frame = FramePosition(static_cast<std::int64_t>(json.value(QStringLiteral("frame")).toDouble()));
    snapshot.timestampMs = static_cast<std::int64_t>(json.value(QStringLiteral("timestampMs")).toDouble());
    snapshot.compositionName = json.value(QStringLiteral("compositionName")).toString();
    snapshot.renderBackend = json.value(QStringLiteral("renderBackend")).toString();
    snapshot.playbackState = json.value(QStringLiteral("playbackState")).toString();
    snapshot.selectedLayerName = json.value(QStringLiteral("selectedLayerName")).toString();
    snapshot.renderLastFrameMs = json.value(QStringLiteral("renderLastFrameMs")).toDouble();
    snapshot.renderAverageFrameMs = json.value(QStringLiteral("renderAverageFrameMs")).toDouble();
    snapshot.renderGpuFrameMs = json.value(QStringLiteral("renderGpuFrameMs")).toDouble();
    snapshot.renderCost.drawCalls = static_cast<std::uint64_t>(json.value(QStringLiteral("renderDrawCalls")).toDouble());
    snapshot.renderCost.indexedDrawCalls = static_cast<std::uint64_t>(json.value(QStringLiteral("renderIndexedDrawCalls")).toDouble());
    snapshot.renderCost.psoSwitches = static_cast<std::uint64_t>(json.value(QStringLiteral("renderPsoSwitches")).toDouble());
    snapshot.renderCost.srbCommits = static_cast<std::uint64_t>(json.value(QStringLiteral("renderSrbCommits")).toDouble());
    snapshot.renderCost.bufferUpdates = static_cast<std::uint64_t>(json.value(QStringLiteral("renderBufferUpdates")).toDouble());
    snapshot.compareMode = compareModeFromString(json.value(QStringLiteral("compareMode")).toString());
    snapshot.compareTargetId = json.value(QStringLiteral("compareTargetId")).toString();
    snapshot.failed = json.value(QStringLiteral("failed")).toBool();
    snapshot.failureReason = json.value(QStringLiteral("failureReason")).toString();

    for (const auto& value : json.value(QStringLiteral("passes")).toArray()) {
        snapshot.passes.push_back(FrameDebugPassRecord::fromJson(value.toObject()));
    }
    for (const auto& value : json.value(QStringLiteral("resources")).toArray()) {
        snapshot.resources.push_back(FrameDebugResourceRecord::fromJson(value.toObject()));
    }
    for (const auto& value : json.value(QStringLiteral("attachments")).toArray()) {
        snapshot.attachments.push_back(FrameDebugAttachmentRecord::fromJson(value.toObject()));
    }
    return snapshot;
}

inline QJsonObject FrameDebugCapture::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("captureId"), captureId);
    json.insert(QStringLiteral("snapshot"), snapshot.toJson());
    json.insert(QStringLiteral("sourceFrameId"), sourceFrameId);
    json.insert(QStringLiteral("targetFrameId"), targetFrameId);
    json.insert(QStringLiteral("pinned"), pinned);
    return json;
}

inline FrameDebugCapture FrameDebugCapture::fromJson(const QJsonObject& json)
{
    FrameDebugCapture capture;
    capture.captureId = json.value(QStringLiteral("captureId")).toString();
    capture.snapshot = FrameDebugSnapshot::fromJson(json.value(QStringLiteral("snapshot")).toObject());
    capture.sourceFrameId = json.value(QStringLiteral("sourceFrameId")).toString();
    capture.targetFrameId = json.value(QStringLiteral("targetFrameId")).toString();
    capture.pinned = json.value(QStringLiteral("pinned")).toBool();
    return capture;
}

inline QJsonObject FrameDebugBundle::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("bundleId"), bundleId);
    json.insert(QStringLiteral("label"), label);
    json.insert(QStringLiteral("createdAtMs"), static_cast<double>(createdAtMs));
    json.insert(QStringLiteral("capture"), capture.toJson());

    QJsonArray historyJson;
    for (const auto& entry : history) {
        historyJson.append(entry.toJson());
    }
    json.insert(QStringLiteral("history"), historyJson);
    return json;
}

inline FrameDebugBundle FrameDebugBundle::fromJson(const QJsonObject& json)
{
    FrameDebugBundle bundle;
    bundle.bundleId = json.value(QStringLiteral("bundleId")).toString();
    bundle.label = json.value(QStringLiteral("label")).toString();
    bundle.createdAtMs = static_cast<std::int64_t>(json.value(QStringLiteral("createdAtMs")).toDouble());
    bundle.capture = FrameDebugCapture::fromJson(json.value(QStringLiteral("capture")).toObject());

    for (const auto& value : json.value(QStringLiteral("history")).toArray()) {
        bundle.history.push_back(FrameDebugCapture::fromJson(value.toObject()));
    }
    return bundle;
}

} // namespace ArtifactCore
