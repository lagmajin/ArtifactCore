module;

#include <cstdint>
#include <QImage>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <utility>
#include <vector>

export module Frame.Debug;

import Frame.Position;
import Graphics.RenderGraph;

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

inline QJsonObject renderGraphDiagnosticToJson(
    const RenderGraphDiagnosticSnapshot& snapshot);
inline RenderGraphDiagnosticSnapshot renderGraphDiagnosticFromJson(
    const QJsonObject& json);

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

struct FrameDebugBindingRecord {
    QString key;
    QString value;
    QString stage;
    QString note;

    [[nodiscard]] QJsonObject toJson() const;
    static FrameDebugBindingRecord fromJson(const QJsonObject& json);
};

struct FrameDebugImagePreviewRecord {
    QString key;
    QString label;
    QString note;
    QImage beforeImage;
    QImage afterImage;
    QImage alphaImage;
    QImage diffImage;
};

struct FrameDebugPassRecord {
    QString name;
    FrameDebugPassKind kind = FrameDebugPassKind::Unknown;
    FrameDebugPassStatus status = FrameDebugPassStatus::Unknown;
    std::int64_t durationUs = 0;
    QString backend;
    QString shaderName;
    QString previewResourceLabel;
    std::vector<FrameDebugAttachmentRecord> inputs;
    std::vector<FrameDebugAttachmentRecord> outputs;
    std::vector<FrameDebugBindingRecord> debugBindings;
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
    int totalLayerCount = 0;
    int visibleLayerCount = 0;
    int textLayerCount = 0;
    int selectedLayerMaskCount = 0;
    int selectedLayerEffectCount = 0;
    int selectedLayerMatteCount = 0;
    double visualDensityScore = 0.0;
    double informationDensityScore = 0.0;
    double luminanceDensityScore = 0.0;
    double motionDensityScore = 0.0;
    QString densityLabel;
    QString densityWarning;
    QString densityNextAction;
    double renderLastFrameMs = 0.0;
    double renderAverageFrameMs = 0.0;
    double renderGpuFrameMs = 0.0;
    bool renderGpuTimingAvailable = false;
    std::uint64_t renderGpuTimingExecutionId = 0;
    RenderCostStats renderCost;
    FrameDebugCompareMode compareMode = FrameDebugCompareMode::Disabled;
    QString compareTargetId;
    std::vector<FrameDebugPassRecord> passes;
    std::vector<FrameDebugResourceRecord> resources;
    std::vector<FrameDebugAttachmentRecord> attachments;
    std::vector<FrameDebugImagePreviewRecord> previews;
    RenderGraphDiagnosticSnapshot renderGraphDiagnostic;
    bool hasRenderGraphDiagnostic = false;
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

inline QJsonObject FrameDebugBindingRecord::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("key"), key);
    json.insert(QStringLiteral("value"), value);
    json.insert(QStringLiteral("stage"), stage);
    json.insert(QStringLiteral("note"), note);
    return json;
}

inline FrameDebugBindingRecord FrameDebugBindingRecord::fromJson(const QJsonObject& json)
{
    FrameDebugBindingRecord record;
    record.key = json.value(QStringLiteral("key")).toString();
    record.value = json.value(QStringLiteral("value")).toString();
    record.stage = json.value(QStringLiteral("stage")).toString();
    record.note = json.value(QStringLiteral("note")).toString();
    return record;
}

inline QJsonObject FrameDebugPassRecord::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("name"), name);
    json.insert(QStringLiteral("kind"), toString(kind));
    json.insert(QStringLiteral("status"), toString(status));
    json.insert(QStringLiteral("durationUs"), static_cast<double>(durationUs));
    json.insert(QStringLiteral("backend"), backend);
    json.insert(QStringLiteral("shaderName"), shaderName);
    json.insert(QStringLiteral("previewResourceLabel"), previewResourceLabel);
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

    QJsonArray bindingsJson;
    for (const auto& binding : debugBindings) {
        bindingsJson.append(binding.toJson());
    }
    json.insert(QStringLiteral("debugBindings"), bindingsJson);
    return json;
}

inline FrameDebugPassRecord FrameDebugPassRecord::fromJson(const QJsonObject& json)
{
    FrameDebugPassRecord record;
    record.name = json.value(QStringLiteral("name")).toString();
    record.kind = passKindFromString(json.value(QStringLiteral("kind")).toString());
    record.status = passStatusFromString(json.value(QStringLiteral("status")).toString());
    record.durationUs = static_cast<std::int64_t>(json.value(QStringLiteral("durationUs")).toDouble());
    record.backend = json.value(QStringLiteral("backend")).toString();
    record.shaderName = json.value(QStringLiteral("shaderName")).toString();
    record.previewResourceLabel = json.value(QStringLiteral("previewResourceLabel")).toString();
    record.note = json.value(QStringLiteral("note")).toString();

    for (const auto& value : json.value(QStringLiteral("inputs")).toArray()) {
        record.inputs.push_back(FrameDebugAttachmentRecord::fromJson(value.toObject()));
    }
    for (const auto& value : json.value(QStringLiteral("outputs")).toArray()) {
        record.outputs.push_back(FrameDebugAttachmentRecord::fromJson(value.toObject()));
    }
    for (const auto& value : json.value(QStringLiteral("debugBindings")).toArray()) {
        record.debugBindings.push_back(FrameDebugBindingRecord::fromJson(value.toObject()));
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
    json.insert(QStringLiteral("totalLayerCount"), totalLayerCount);
    json.insert(QStringLiteral("visibleLayerCount"), visibleLayerCount);
    json.insert(QStringLiteral("textLayerCount"), textLayerCount);
    json.insert(QStringLiteral("selectedLayerMaskCount"), selectedLayerMaskCount);
    json.insert(QStringLiteral("selectedLayerEffectCount"), selectedLayerEffectCount);
    json.insert(QStringLiteral("selectedLayerMatteCount"), selectedLayerMatteCount);
    json.insert(QStringLiteral("visualDensityScore"), visualDensityScore);
    json.insert(QStringLiteral("informationDensityScore"), informationDensityScore);
    json.insert(QStringLiteral("luminanceDensityScore"), luminanceDensityScore);
    json.insert(QStringLiteral("motionDensityScore"), motionDensityScore);
    json.insert(QStringLiteral("densityLabel"), densityLabel);
    json.insert(QStringLiteral("densityWarning"), densityWarning);
    json.insert(QStringLiteral("densityNextAction"), densityNextAction);
    json.insert(QStringLiteral("renderLastFrameMs"), renderLastFrameMs);
    json.insert(QStringLiteral("renderAverageFrameMs"), renderAverageFrameMs);
    json.insert(QStringLiteral("renderGpuFrameMs"), renderGpuFrameMs);
    json.insert(QStringLiteral("renderGpuTimingAvailable"), renderGpuTimingAvailable);
    json.insert(QStringLiteral("renderGpuTimingExecutionId"),
                QString::number(renderGpuTimingExecutionId));
    json.insert(QStringLiteral("renderDrawCalls"), static_cast<double>(renderCost.drawCalls));
    json.insert(QStringLiteral("renderIndexedDrawCalls"), static_cast<double>(renderCost.indexedDrawCalls));
    json.insert(QStringLiteral("renderPsoSwitches"), static_cast<double>(renderCost.psoSwitches));
    json.insert(QStringLiteral("renderSrbCommits"), static_cast<double>(renderCost.srbCommits));
    json.insert(QStringLiteral("renderBufferUpdates"), static_cast<double>(renderCost.bufferUpdates));
    json.insert(QStringLiteral("compareMode"), toString(compareMode));
    json.insert(QStringLiteral("compareTargetId"), compareTargetId);
    json.insert(QStringLiteral("hasRenderGraphDiagnostic"), hasRenderGraphDiagnostic);
    if (hasRenderGraphDiagnostic) {
        json.insert(QStringLiteral("renderGraphDiagnostic"),
                    renderGraphDiagnosticToJson(renderGraphDiagnostic));
    }
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
    snapshot.totalLayerCount = json.value(QStringLiteral("totalLayerCount")).toInt();
    snapshot.visibleLayerCount = json.value(QStringLiteral("visibleLayerCount")).toInt();
    snapshot.textLayerCount = json.value(QStringLiteral("textLayerCount")).toInt();
    snapshot.selectedLayerMaskCount = json.value(QStringLiteral("selectedLayerMaskCount")).toInt();
    snapshot.selectedLayerEffectCount = json.value(QStringLiteral("selectedLayerEffectCount")).toInt();
    snapshot.selectedLayerMatteCount = json.value(QStringLiteral("selectedLayerMatteCount")).toInt();
    snapshot.visualDensityScore = json.value(QStringLiteral("visualDensityScore")).toDouble();
    snapshot.informationDensityScore = json.value(QStringLiteral("informationDensityScore")).toDouble();
    snapshot.luminanceDensityScore = json.value(QStringLiteral("luminanceDensityScore")).toDouble();
    snapshot.motionDensityScore = json.value(QStringLiteral("motionDensityScore")).toDouble();
    snapshot.densityLabel = json.value(QStringLiteral("densityLabel")).toString();
    snapshot.densityWarning = json.value(QStringLiteral("densityWarning")).toString();
    snapshot.densityNextAction = json.value(QStringLiteral("densityNextAction")).toString();
    snapshot.renderLastFrameMs = json.value(QStringLiteral("renderLastFrameMs")).toDouble();
    snapshot.renderAverageFrameMs = json.value(QStringLiteral("renderAverageFrameMs")).toDouble();
    snapshot.renderGpuFrameMs = json.value(QStringLiteral("renderGpuFrameMs")).toDouble();
    snapshot.renderGpuTimingAvailable = json.value(
        QStringLiteral("renderGpuTimingAvailable")).toBool();
    snapshot.renderGpuTimingExecutionId = json.value(
        QStringLiteral("renderGpuTimingExecutionId")).toString().toULongLong();
    snapshot.renderCost.drawCalls = static_cast<std::uint64_t>(json.value(QStringLiteral("renderDrawCalls")).toDouble());
    snapshot.renderCost.indexedDrawCalls = static_cast<std::uint64_t>(json.value(QStringLiteral("renderIndexedDrawCalls")).toDouble());
    snapshot.renderCost.psoSwitches = static_cast<std::uint64_t>(json.value(QStringLiteral("renderPsoSwitches")).toDouble());
    snapshot.renderCost.srbCommits = static_cast<std::uint64_t>(json.value(QStringLiteral("renderSrbCommits")).toDouble());
    snapshot.renderCost.bufferUpdates = static_cast<std::uint64_t>(json.value(QStringLiteral("renderBufferUpdates")).toDouble());
    snapshot.compareMode = compareModeFromString(json.value(QStringLiteral("compareMode")).toString());
    snapshot.compareTargetId = json.value(QStringLiteral("compareTargetId")).toString();
    snapshot.hasRenderGraphDiagnostic = json.value(
        QStringLiteral("hasRenderGraphDiagnostic")).toBool();
    if (snapshot.hasRenderGraphDiagnostic) {
        snapshot.renderGraphDiagnostic = renderGraphDiagnosticFromJson(
            json.value(QStringLiteral("renderGraphDiagnostic")).toObject());
    }
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

inline QString toString(const RenderResourceKind kind)
{
    return kind == RenderResourceKind::Texture
        ? QStringLiteral("Texture")
        : QStringLiteral("Buffer");
}

inline QString toString(const RenderResourceLifetime lifetime)
{
    switch (lifetime) {
    case RenderResourceLifetime::Persistent: return QStringLiteral("Persistent");
    case RenderResourceLifetime::External: return QStringLiteral("External");
    default: return QStringLiteral("Transient");
    }
}

inline QString toString(const RenderPassQueue queue)
{
    switch (queue) {
    case RenderPassQueue::Compute: return QStringLiteral("Compute");
    case RenderPassQueue::Copy: return QStringLiteral("Copy");
    default: return QStringLiteral("Graphics");
    }
}

inline QString toString(const RenderDiagnosticPassState state)
{
    switch (state) {
    case RenderDiagnosticPassState::Scheduled: return QStringLiteral("Scheduled");
    case RenderDiagnosticPassState::Disabled: return QStringLiteral("Disabled");
    default: return QStringLiteral("Blocked");
    }
}

inline QJsonObject renderGraphDiagnosticToJson(
    const RenderGraphDiagnosticSnapshot& snapshot)
{
    QJsonObject json;
    json.insert(QStringLiteral("executionId"), QString::number(snapshot.executionId));
    json.insert(QStringLiteral("valid"), snapshot.valid);
    json.insert(QStringLiteral("error"), QString::fromStdString(snapshot.error));
    json.insert(QStringLiteral("estimatedResourceBytes"),
                QString::number(snapshot.estimatedResourceBytes));

    QJsonArray passesJson;
    for (const auto& pass : snapshot.passes) {
        QJsonObject passJson;
        passJson.insert(QStringLiteral("id"), static_cast<int>(pass.handle.id));
        passJson.insert(QStringLiteral("name"), QString::fromStdString(pass.descriptor.name));
        passJson.insert(QStringLiteral("queue"), toString(pass.descriptor.queue));
        passJson.insert(QStringLiteral("state"), toString(pass.state));
        passJson.insert(QStringLiteral("executionOrder"),
                        static_cast<qint64>(pass.executionOrder));
        passJson.insert(QStringLiteral("gpuDurationUs"), QString::number(pass.gpuDurationUs));
        passJson.insert(QStringLiteral("gpuSampleExecutionId"),
                        QString::number(pass.gpuSampleExecutionId));
        passJson.insert(QStringLiteral("gpuTimingAvailable"), pass.gpuTimingAvailable);

        QJsonArray readsJson;
        for (const auto handle : pass.descriptor.reads) {
            readsJson.append(static_cast<int>(handle.id));
        }
        passJson.insert(QStringLiteral("reads"), readsJson);

        QJsonArray writesJson;
        for (const auto handle : pass.descriptor.writes) {
            writesJson.append(static_cast<int>(handle.id));
        }
        passJson.insert(QStringLiteral("writes"), writesJson);
        passesJson.append(passJson);
    }
    json.insert(QStringLiteral("passes"), passesJson);

    QJsonArray resourcesJson;
    for (const auto& resource : snapshot.resources) {
        QJsonObject resourceJson;
        resourceJson.insert(QStringLiteral("id"), static_cast<int>(resource.handle.id));
        resourceJson.insert(QStringLiteral("name"),
                            QString::fromStdString(resource.descriptor.name));
        resourceJson.insert(QStringLiteral("kind"), toString(resource.descriptor.kind));
        resourceJson.insert(QStringLiteral("lifetime"),
                            toString(resource.descriptor.lifetime));
        resourceJson.insert(QStringLiteral("width"),
                            static_cast<int>(resource.descriptor.width));
        resourceJson.insert(QStringLiteral("height"),
                            static_cast<int>(resource.descriptor.height));
        resourceJson.insert(QStringLiteral("depth"),
                            static_cast<int>(resource.descriptor.depth));
        resourceJson.insert(QStringLiteral("format"),
                            static_cast<int>(resource.descriptor.format));
        resourceJson.insert(QStringLiteral("byteSize"),
                            QString::number(resource.descriptor.byteSize));
        resourceJson.insert(QStringLiteral("used"), resource.used);
        if (resource.used) {
            resourceJson.insert(QStringLiteral("firstPass"),
                                static_cast<qint64>(resource.firstPass));
            resourceJson.insert(QStringLiteral("lastPass"),
                                static_cast<qint64>(resource.lastPass));
        }
        resourcesJson.append(resourceJson);
    }
    json.insert(QStringLiteral("resources"), resourcesJson);
    return json;
}

inline RenderGraphDiagnosticSnapshot renderGraphDiagnosticFromJson(
    const QJsonObject& json)
{
    RenderGraphDiagnosticSnapshot snapshot;
    snapshot.executionId = json.value(QStringLiteral("executionId")).toString().toULongLong();
    snapshot.valid = json.value(QStringLiteral("valid")).toBool();
    snapshot.error = json.value(QStringLiteral("error")).toString().toStdString();
    snapshot.estimatedResourceBytes = json.value(
        QStringLiteral("estimatedResourceBytes")).toString().toULongLong();

    for (const auto& value : json.value(QStringLiteral("passes")).toArray()) {
        const auto passJson = value.toObject();
        RenderDiagnosticPassRecord pass;
        pass.handle.id = static_cast<std::uint32_t>(passJson.value(
            QStringLiteral("id")).toInt());
        pass.descriptor.name = passJson.value(QStringLiteral("name")).toString().toStdString();
        const auto queue = passJson.value(QStringLiteral("queue")).toString();
        if (queue == QStringLiteral("Compute")) pass.descriptor.queue = RenderPassQueue::Compute;
        else if (queue == QStringLiteral("Copy")) pass.descriptor.queue = RenderPassQueue::Copy;
        else pass.descriptor.queue = RenderPassQueue::Graphics;
        const auto state = passJson.value(QStringLiteral("state")).toString();
        if (state == QStringLiteral("Scheduled")) {
            pass.state = RenderDiagnosticPassState::Scheduled;
        } else if (state == QStringLiteral("Disabled")) {
            pass.state = RenderDiagnosticPassState::Disabled;
        } else {
            pass.state = RenderDiagnosticPassState::Blocked;
        }
        pass.executionOrder = static_cast<std::size_t>(passJson.value(
            QStringLiteral("executionOrder")).toDouble());
        pass.gpuDurationUs = passJson.value(QStringLiteral("gpuDurationUs")).toString().toULongLong();
        pass.gpuSampleExecutionId = passJson.value(
            QStringLiteral("gpuSampleExecutionId")).toString().toULongLong();
        pass.gpuTimingAvailable = passJson.value(
            QStringLiteral("gpuTimingAvailable")).toBool();
        for (const auto read : passJson.value(QStringLiteral("reads")).toArray()) {
            pass.descriptor.reads.push_back({static_cast<std::uint32_t>(read.toInt())});
        }
        for (const auto write : passJson.value(QStringLiteral("writes")).toArray()) {
            pass.descriptor.writes.push_back({static_cast<std::uint32_t>(write.toInt())});
        }
        pass.descriptor.enabled = pass.state != RenderDiagnosticPassState::Disabled;
        snapshot.passes.push_back(std::move(pass));
    }

    for (const auto& value : json.value(QStringLiteral("resources")).toArray()) {
        const auto resourceJson = value.toObject();
        RenderDiagnosticResourceRecord resource;
        resource.handle.id = static_cast<std::uint32_t>(resourceJson.value(
            QStringLiteral("id")).toInt());
        resource.descriptor.name = resourceJson.value(
            QStringLiteral("name")).toString().toStdString();
        resource.descriptor.kind = resourceJson.value(QStringLiteral("kind")).toString()
            == QStringLiteral("Buffer") ? RenderResourceKind::Buffer : RenderResourceKind::Texture;
        const auto lifetime = resourceJson.value(QStringLiteral("lifetime")).toString();
        if (lifetime == QStringLiteral("Persistent")) {
            resource.descriptor.lifetime = RenderResourceLifetime::Persistent;
        } else if (lifetime == QStringLiteral("External")) {
            resource.descriptor.lifetime = RenderResourceLifetime::External;
        } else {
            resource.descriptor.lifetime = RenderResourceLifetime::Transient;
        }
        resource.descriptor.width = static_cast<std::uint32_t>(resourceJson.value(
            QStringLiteral("width")).toInt());
        resource.descriptor.height = static_cast<std::uint32_t>(resourceJson.value(
            QStringLiteral("height")).toInt());
        resource.descriptor.depth = static_cast<std::uint32_t>(resourceJson.value(
            QStringLiteral("depth")).toInt(1));
        resource.descriptor.format = static_cast<std::uint32_t>(resourceJson.value(
            QStringLiteral("format")).toInt());
        resource.descriptor.byteSize = resourceJson.value(
            QStringLiteral("byteSize")).toString().toULongLong();
        resource.used = resourceJson.value(QStringLiteral("used")).toBool();
        resource.firstPass = static_cast<std::size_t>(resourceJson.value(
            QStringLiteral("firstPass")).toDouble());
        resource.lastPass = static_cast<std::size_t>(resourceJson.value(
            QStringLiteral("lastPass")).toDouble());
        snapshot.resources.push_back(std::move(resource));
    }
    return snapshot;
}

} // namespace ArtifactCore
