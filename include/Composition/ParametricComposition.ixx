module;

#include <functional>
#include <memory>
#include <optional>

#include <QByteArray>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QVariant>

export module Composition.ParametricComposition;

import FloatRGBA;
import Image.ImageF32x4_RGBA;
import Utils.Id;

export namespace ArtifactCore {

/// ParametricComposition は Slot ベースの再利用可能な composition 定義です。
/// 初期段階は 1 input / 1 output / RGBA に限定しつつ、型としては Input / Output / Control / Event を表現できるようにしてある。
/// - 入力未接続時は透明 RGBA を返す
/// - 公開パラメータは instance 側で override 可能
/// - 循環参照は binding の ancestry で明示的に拒否する
/// - cache key は definitionId + input frame hash + parameter values + time
enum class ParametricCompositionSlotKind {
    SourceLayer,
    Image,
    Text,
    Matte,
    RGBA,
    Alpha,
    MotionPath,
    Bool,
    Control,
    Event
};

enum class ParametricCompositionSlotRole {
    Input,
    Output,
    Control,
    Event
};

struct ParametricCompositionSlot {
    QString slotId;
    QString displayName;
    ParametricCompositionSlotRole role = ParametricCompositionSlotRole::Input;
    ParametricCompositionSlotKind kind = ParametricCompositionSlotKind::SourceLayer;
    QString valueType = QStringLiteral("RGBA");
    bool required = true;

    QJsonObject toJson() const;
    static ParametricCompositionSlot fromJson(const QJsonObject& obj);
};

using ParametricCompositionInputSlot = ParametricCompositionSlot;
using ParametricCompositionOutputSlot = ParametricCompositionSlot;
using ParametricCompositionControlSlot = ParametricCompositionSlot;
using ParametricCompositionEventSlot = ParametricCompositionSlot;

struct ParametricCompositionParameter {
    QString key;
    QString displayName;
    QVariant defaultValue;
    bool overridableByInstance = true;
    bool visible = true;

    QJsonObject toJson() const;
    static ParametricCompositionParameter fromJson(const QJsonObject& obj);
};

struct ParametricCompositionInputBinding {
    ParametricCompositionSlotKind kind = ParametricCompositionSlotKind::SourceLayer;
    bool connected = false;
    QString slotId;
    LayerID sourceLayerId;
    QString sourceDefinitionId;
    QStringList upstreamDefinitionIds;
    ImageF32x4_RGBA image;
    QString text;
    ImageF32x4_RGBA matte;

    static ParametricCompositionInputBinding fromSourceLayer(
        const LayerID& layerId,
        const QString& slot = QString(),
        const QString& sourceDefinition = QString(),
        const QStringList& upstreamDefinitions = QStringList()
    );
    static ParametricCompositionInputBinding fromImage(
        const ImageF32x4_RGBA& value,
        const QString& slot = QString()
    );
    static ParametricCompositionInputBinding fromText(
        const QString& value,
        const QString& slot = QString()
    );
    static ParametricCompositionInputBinding fromMatte(
        const ImageF32x4_RGBA& value,
        const QString& slot = QString()
    );

    bool isConnected() const;
    bool wouldCreateCycle(const QString& currentDefinitionId) const;
    QJsonObject toJson() const;
    static ParametricCompositionInputBinding fromJson(const QJsonObject& obj);
};

struct ParametricCompositionRenderContext {
    double timeSeconds = 0.0;
    QSize outputSize;
    qint64 timeKey() const;
};

struct ParametricCompositionCacheKey {
    QString definitionId;
    QMap<QString, QByteArray> inputFrameHashes;
    QByteArray parameterHash;
    qint64 timeKey = 0;

    bool isValid() const;
    QString toString() const;
};

struct ParametricCompositionEvaluation {
    ImageF32x4_RGBA image;
    QMap<QString, ImageF32x4_RGBA> resolvedInputs;
    bool inputResolved = false;
    bool usedTransparentFallback = false;
    ParametricCompositionCacheKey cacheKey;
};

struct ParametricCompositionBundle {
    QString bundleKind = QStringLiteral("parametric-composition");
    QString bundleTitle;
    QJsonObject definition;
    QJsonObject instance;
    QJsonObject metadata;

    QJsonObject toJson() const;
    static ParametricCompositionBundle fromJson(const QJsonObject& obj);
};

QJsonObject parametricCompositionBundleToCompositionJson(const ParametricCompositionBundle& bundle);

class ParametricCompositionDefinition;

ParametricCompositionDefinition makeDefaultParametricCompositionDefinition(
    QString definitionId,
    QString displayName = QString(),
    QString inputSlotId = QStringLiteral("input"),
    QString outputSlotId = QStringLiteral("output"));

struct ParametricCompositionInputResolver {
    using Resolver = std::function<std::optional<ImageF32x4_RGBA>(
        const ParametricCompositionInputBinding& binding,
        const ParametricCompositionRenderContext& context)>;

    Resolver resolve;
};

class ParametricCompositionDefinition {
public:
    ParametricCompositionDefinition() = default;
    explicit ParametricCompositionDefinition(QString definitionId, QString displayName = QString());

    const QString& definitionId() const;
    void setDefinitionId(const QString& id);

    const QString& displayName() const;
    void setDisplayName(const QString& name);

    const QVector<ParametricCompositionSlot>& slotDefinitions() const;
    QVector<ParametricCompositionSlot> inputSlots() const;
    QVector<ParametricCompositionSlot> outputSlots() const;
    const ParametricCompositionSlot* slot(const QString& slotId) const;
    bool hasSlot(const QString& slotId) const;
    const QVector<ParametricCompositionParameter>& parameters() const;

    bool addSlot(const ParametricCompositionSlot& slot);
    bool setSlot(const ParametricCompositionSlot& slot);
    bool removeSlot(const QString& slotId);
    void clearSlots();
    QVector<ParametricCompositionSlot> slotsByRole(ParametricCompositionSlotRole role) const;

    bool addParameter(const ParametricCompositionParameter& parameter);
    bool removeParameter(const QString& key);
    bool hasParameter(const QString& key) const;
    const ParametricCompositionParameter* parameter(const QString& key) const;

    bool validate(QString* errorMessage = nullptr) const;

    QJsonObject toJson() const;
    static ParametricCompositionDefinition fromJson(const QJsonObject& obj);

private:
    QString definitionId_;
    QString displayName_;
    QVector<ParametricCompositionSlot> slots_;
    QVector<ParametricCompositionParameter> parameters_;
};

class ParametricCompositionInstance {
public:
    ParametricCompositionInstance() = default;
    explicit ParametricCompositionInstance(std::shared_ptr<const ParametricCompositionDefinition> definition);

    std::shared_ptr<const ParametricCompositionDefinition> definition() const;
    void setDefinition(std::shared_ptr<const ParametricCompositionDefinition> definition);

    const QVector<ParametricCompositionInputBinding>& inputBindings() const;
    void addInputBinding(const ParametricCompositionInputBinding& binding);
    void setInputBinding(int index, const ParametricCompositionInputBinding& binding);
    void removeInputBinding(int index);
    void clearInputBindings();
    int inputBindingCount() const;
    bool isInputConnected(int index) const;
    bool hasAnyInputConnected() const;

    QVariant parameterValue(const QString& key, const QVariant& fallback = QVariant()) const;
    void setParameterOverride(const QString& key, const QVariant& value);
    void clearParameterOverride(const QString& key);
    const QMap<QString, QVariant>& parameterOverrides() const;

    QMap<QString, QVariant> resolvedParameters() const;
    QByteArray parameterHash() const;
    QByteArray inputFrameHash(
        const ParametricCompositionRenderContext& context,
        const ParametricCompositionInputResolver& resolver
    ) const;
    ParametricCompositionCacheKey cacheKey(
        const ParametricCompositionRenderContext& context,
        const ParametricCompositionInputResolver& resolver
    ) const;

    ParametricCompositionEvaluation evaluate(
        const ParametricCompositionRenderContext& context,
        const ParametricCompositionInputResolver& resolver
    ) const;

    ImageF32x4_RGBA render(
        const ParametricCompositionRenderContext& context,
        const ParametricCompositionInputResolver& resolver
    ) const;

    QJsonObject toJson() const;
    static ParametricCompositionInstance fromJson(
        const QJsonObject& obj,
        std::shared_ptr<const ParametricCompositionDefinition> definition = {}
    );

private:
    static ImageF32x4_RGBA makeTransparent(const QSize& size);
    static QByteArray hashImage(const ImageF32x4_RGBA& image);
    static QByteArray hashVariantMap(const QMap<QString, QVariant>& values);

    std::shared_ptr<const ParametricCompositionDefinition> definition_;
    QVector<ParametricCompositionInputBinding> inputBindings_;
    QMap<QString, QVariant> parameterOverrides_;
};

} // namespace ArtifactCore
