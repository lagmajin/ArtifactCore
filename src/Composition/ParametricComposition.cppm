module;

#include <cmath>
#include <memory>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>

module Composition.ParametricComposition;

namespace ArtifactCore {

namespace {

QVariant variantFromJsonValue(const QJsonValue& value)
{
    if (value.isArray()) {
        return QVariant::fromValue(value.toArray().toVariantList());
    }
    if (value.isObject()) {
        return QVariant::fromValue(value.toObject().toVariantMap());
    }
    return value.toVariant();
}

QJsonValue jsonValueFromVariant(const QVariant& value)
{
    return QJsonValue::fromVariant(value);
}

} // namespace

QJsonObject ParametricCompositionSlot::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("slotId"), slotId);
    obj.insert(QStringLiteral("displayName"), displayName);
    obj.insert(QStringLiteral("role"), static_cast<int>(role));
    obj.insert(QStringLiteral("kind"), static_cast<int>(kind));
    obj.insert(QStringLiteral("valueType"), valueType);
    obj.insert(QStringLiteral("required"), required);
    return obj;
}

ParametricCompositionSlot ParametricCompositionSlot::fromJson(const QJsonObject& obj)
{
    ParametricCompositionSlot slot;
    slot.slotId = obj.value(QStringLiteral("slotId")).toString();
    slot.displayName = obj.value(QStringLiteral("displayName")).toString();
    slot.role = static_cast<ParametricCompositionSlotRole>(
        obj.value(QStringLiteral("role")).toInt(static_cast<int>(ParametricCompositionSlotRole::Input)));
    slot.kind = static_cast<ParametricCompositionSlotKind>(
        obj.value(QStringLiteral("kind")).toInt(static_cast<int>(ParametricCompositionSlotKind::SourceLayer)));
    slot.valueType = obj.value(QStringLiteral("valueType")).toString(QStringLiteral("RGBA"));
    slot.required = obj.value(QStringLiteral("required")).toBool(true);
    return slot;
}

QJsonObject ParametricCompositionParameter::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("key"), key);
    obj.insert(QStringLiteral("displayName"), displayName);
    obj.insert(QStringLiteral("defaultValue"), jsonValueFromVariant(defaultValue));
    obj.insert(QStringLiteral("overridableByInstance"), overridableByInstance);
    obj.insert(QStringLiteral("visible"), visible);
    return obj;
}

ParametricCompositionParameter ParametricCompositionParameter::fromJson(const QJsonObject& obj)
{
    ParametricCompositionParameter parameter;
    parameter.key = obj.value(QStringLiteral("key")).toString();
    parameter.displayName = obj.value(QStringLiteral("displayName")).toString();
    parameter.defaultValue = variantFromJsonValue(obj.value(QStringLiteral("defaultValue")));
    parameter.overridableByInstance = obj.value(QStringLiteral("overridableByInstance")).toBool(true);
    parameter.visible = obj.value(QStringLiteral("visible")).toBool(true);
    return parameter;
}

ParametricCompositionInputBinding ParametricCompositionInputBinding::fromSourceLayer(
    const LayerID& layerId,
    const QString& slot,
    const QString& sourceDefinition,
    const QStringList& upstreamDefinitions)
{
    ParametricCompositionInputBinding binding;
    binding.kind = ParametricCompositionSlotKind::SourceLayer;
    binding.connected = !layerId.isNil();
    binding.slotId = slot;
    binding.sourceLayerId = layerId;
    binding.sourceDefinitionId = sourceDefinition;
    binding.upstreamDefinitionIds = upstreamDefinitions;
    return binding;
}

ParametricCompositionInputBinding ParametricCompositionInputBinding::fromImage(
    const ImageF32x4_RGBA& value,
    const QString& slot)
{
    ParametricCompositionInputBinding binding;
    binding.kind = ParametricCompositionSlotKind::Image;
    binding.connected = !value.isEmpty();
    binding.slotId = slot;
    binding.image = value;
    return binding;
}

ParametricCompositionInputBinding ParametricCompositionInputBinding::fromText(
    const QString& value,
    const QString& slot)
{
    ParametricCompositionInputBinding binding;
    binding.kind = ParametricCompositionSlotKind::Text;
    binding.connected = !value.isEmpty();
    binding.slotId = slot;
    binding.text = value;
    return binding;
}

ParametricCompositionInputBinding ParametricCompositionInputBinding::fromMatte(
    const ImageF32x4_RGBA& value,
    const QString& slot)
{
    ParametricCompositionInputBinding binding;
    binding.kind = ParametricCompositionSlotKind::Matte;
    binding.connected = !value.isEmpty();
    binding.slotId = slot;
    binding.matte = value;
    return binding;
}

bool ParametricCompositionInputBinding::isConnected() const
{
    return connected;
}

bool ParametricCompositionInputBinding::wouldCreateCycle(const QString& currentDefinitionId) const
{
    if (currentDefinitionId.isEmpty()) {
        return false;
    }
    if (sourceDefinitionId == currentDefinitionId) {
        return true;
    }
    return upstreamDefinitionIds.contains(currentDefinitionId);
}

QJsonObject ParametricCompositionInputBinding::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("kind"), static_cast<int>(kind));
    obj.insert(QStringLiteral("connected"), connected);
    obj.insert(QStringLiteral("slotId"), slotId);
    obj.insert(QStringLiteral("sourceLayerId"), sourceLayerId.toString());
    obj.insert(QStringLiteral("sourceDefinitionId"), sourceDefinitionId);
    obj.insert(QStringLiteral("upstreamDefinitionIds"), QJsonArray::fromStringList(upstreamDefinitionIds));
    obj.insert(QStringLiteral("hasImage"), !image.isEmpty());
    obj.insert(QStringLiteral("hasMatte"), !matte.isEmpty());
    obj.insert(QStringLiteral("text"), text);
    return obj;
}

ParametricCompositionInputBinding ParametricCompositionInputBinding::fromJson(const QJsonObject& obj)
{
    ParametricCompositionInputBinding binding;
    binding.kind = static_cast<ParametricCompositionSlotKind>(
        obj.value(QStringLiteral("kind")).toInt(static_cast<int>(ParametricCompositionSlotKind::SourceLayer)));
    binding.connected = obj.value(QStringLiteral("connected")).toBool(false);
    binding.slotId = obj.value(QStringLiteral("slotId")).toString();
    binding.sourceLayerId = LayerID(obj.value(QStringLiteral("sourceLayerId")).toString());
    binding.sourceDefinitionId = obj.value(QStringLiteral("sourceDefinitionId")).toString();
    const QJsonArray upstream = obj.value(QStringLiteral("upstreamDefinitionIds")).toArray();
    for (const auto& value : upstream) {
        binding.upstreamDefinitionIds.append(value.toString());
    }
    const bool hasImage = obj.value(QStringLiteral("hasImage")).toBool(false);
    const bool hasMatte = obj.value(QStringLiteral("hasMatte")).toBool(false);
    if (binding.kind == ParametricCompositionSlotKind::Image && !hasImage) {
        binding.connected = false;
    }
    if (binding.kind == ParametricCompositionSlotKind::Matte && !hasMatte) {
        binding.connected = false;
    }
    binding.text = obj.value(QStringLiteral("text")).toString();
    return binding;
}

qint64 ParametricCompositionRenderContext::timeKey() const
{
    return static_cast<qint64>(std::llround(timeSeconds * 1000000.0));
}

bool ParametricCompositionCacheKey::isValid() const
{
    return !definitionId.isEmpty();
}

QString ParametricCompositionCacheKey::toString() const
{
    return QStringLiteral("%1|%2|%3|%4")
        .arg(definitionId)
        .arg(QString::fromLatin1(inputFrameHash.toHex()))
        .arg(QString::fromLatin1(parameterHash.toHex()))
        .arg(timeKey);
}

QJsonObject ParametricCompositionBundle::toJson() const
{
    QJsonObject obj = metadata;
    obj.insert(QStringLiteral("bundleKind"), bundleKind);
    obj.insert(QStringLiteral("bundleTitle"), bundleTitle);
    obj.insert(QStringLiteral("definition"), definition);
    obj.insert(QStringLiteral("instance"), instance);
    return obj;
}

ParametricCompositionBundle ParametricCompositionBundle::fromJson(const QJsonObject& obj)
{
    ParametricCompositionBundle bundle;
    bundle.bundleKind = obj.value(QStringLiteral("bundleKind")).toString(QStringLiteral("parametric-composition"));
    bundle.bundleTitle = obj.value(QStringLiteral("bundleTitle")).toString();
    bundle.definition = obj.value(QStringLiteral("definition")).toObject();
    bundle.instance = obj.value(QStringLiteral("instance")).toObject();
    bundle.metadata = obj;
    bundle.metadata.remove(QStringLiteral("bundleKind"));
    bundle.metadata.remove(QStringLiteral("bundleTitle"));
    bundle.metadata.remove(QStringLiteral("definition"));
    bundle.metadata.remove(QStringLiteral("instance"));
    return bundle;
}

QJsonObject parametricCompositionBundleToCompositionJson(const ParametricCompositionBundle& bundle)
{
    QJsonObject compositionJson = bundle.definition;
    if (compositionJson.isEmpty()) {
        compositionJson = bundle.instance;
    }
    if (compositionJson.isEmpty()) {
        return {};
    }
    if (!bundle.bundleTitle.isEmpty()) {
        compositionJson.insert(QStringLiteral("displayName"), bundle.bundleTitle);
    }
    compositionJson.insert(QStringLiteral("bundleKind"), bundle.bundleKind);
    compositionJson.insert(QStringLiteral("bundleMetadata"), bundle.metadata);
    if (!bundle.instance.isEmpty()) {
        compositionJson.insert(QStringLiteral("instance"), bundle.instance);
    }
    return compositionJson;
}

ParametricCompositionDefinition makeDefaultParametricCompositionDefinition(
    QString definitionId,
    QString displayName,
    QString inputSlotId,
    QString outputSlotId)
{
    ParametricCompositionDefinition definition(std::move(definitionId), std::move(displayName));

    ParametricCompositionSlot inputSlot;
    inputSlot.slotId = std::move(inputSlotId);
    inputSlot.displayName = QStringLiteral("Input");
    inputSlot.role = ParametricCompositionSlotRole::Input;
    inputSlot.kind = ParametricCompositionSlotKind::RGBA;
    inputSlot.valueType = QStringLiteral("RGBA");
    inputSlot.required = true;
    definition.addSlot(inputSlot);

    ParametricCompositionSlot outputSlot;
    outputSlot.slotId = std::move(outputSlotId);
    outputSlot.displayName = QStringLiteral("Output");
    outputSlot.role = ParametricCompositionSlotRole::Output;
    outputSlot.kind = ParametricCompositionSlotKind::RGBA;
    outputSlot.valueType = QStringLiteral("RGBA");
    outputSlot.required = true;
    definition.addSlot(outputSlot);

    return definition;
}

ParametricCompositionDefinition::ParametricCompositionDefinition(QString definitionId, QString displayName)
    : definitionId_(std::move(definitionId))
    , displayName_(std::move(displayName))
{
}

const QString& ParametricCompositionDefinition::definitionId() const
{
    return definitionId_;
}

void ParametricCompositionDefinition::setDefinitionId(const QString& id)
{
    definitionId_ = id;
}

const QString& ParametricCompositionDefinition::displayName() const
{
    return displayName_;
}

void ParametricCompositionDefinition::setDisplayName(const QString& name)
{
    displayName_ = name;
}

const QVector<ParametricCompositionSlot>& ParametricCompositionDefinition::slotDefinitions() const
{
    return slots_;
}

const QVector<ParametricCompositionSlot>& ParametricCompositionDefinition::inputSlots() const
{
    return slots_;
}

QVector<ParametricCompositionSlot> ParametricCompositionDefinition::outputSlots() const
{
    return slotsByRole(ParametricCompositionSlotRole::Output);
}

const QVector<ParametricCompositionParameter>& ParametricCompositionDefinition::parameters() const
{
    return parameters_;
}

bool ParametricCompositionDefinition::addSlot(const ParametricCompositionSlot& slot)
{
    if (slot.slotId.isEmpty()) {
        return false;
    }
    for (const auto& existing : slots_) {
        if (existing.slotId == slot.slotId) {
            return false;
        }
    }
    slots_.append(slot);
    return true;
}

bool ParametricCompositionDefinition::setSlot(const ParametricCompositionSlot& slot)
{
    slots_.clear();
    return addSlot(slot);
}

void ParametricCompositionDefinition::clearSlots()
{
    slots_.clear();
}

QVector<ParametricCompositionSlot> ParametricCompositionDefinition::slotsByRole(ParametricCompositionSlotRole role) const
{
    QVector<ParametricCompositionSlot> filtered;
    for (const auto& slot : slots_) {
        if (slot.role == role) {
            filtered.append(slot);
        }
    }
    return filtered;
}

bool ParametricCompositionDefinition::addParameter(const ParametricCompositionParameter& parameter)
{
    if (parameter.key.isEmpty()) {
        return false;
    }
    for (const auto& existing : parameters_) {
        if (existing.key == parameter.key) {
            return false;
        }
    }
    parameters_.append(parameter);
    return true;
}

bool ParametricCompositionDefinition::removeParameter(const QString& key)
{
    for (auto it = parameters_.begin(); it != parameters_.end(); ++it) {
        if (it->key == key) {
            parameters_.erase(it);
            return true;
        }
    }
    return false;
}

bool ParametricCompositionDefinition::hasParameter(const QString& key) const
{
    return parameter(key) != nullptr;
}

const ParametricCompositionParameter* ParametricCompositionDefinition::parameter(const QString& key) const
{
    for (const auto& item : parameters_) {
        if (item.key == key) {
            return &item;
        }
    }
    return nullptr;
}

bool ParametricCompositionDefinition::validate(QString* errorMessage) const
{
    if (definitionId_.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("ParametricComposition requires a definitionId.");
        }
        return false;
    }
    const auto inputOnlySlots = slotsByRole(ParametricCompositionSlotRole::Input);
    const auto outputOnlySlots = slotsByRole(ParametricCompositionSlotRole::Output);
    if (inputOnlySlots.size() != 1) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("ParametricComposition requires exactly one input slot.");
        }
        return false;
    }
    if (outputOnlySlots.size() != 1) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("ParametricComposition requires exactly one output slot.");
        }
        return false;
    }
    if (inputOnlySlots.front().slotId.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("ParametricComposition input slotId cannot be empty.");
        }
        return false;
    }
    if (outputOnlySlots.front().slotId.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("ParametricComposition output slotId cannot be empty.");
        }
        return false;
    }
    for (const auto& slot : slots_) {
        if (slot.valueType.compare(QStringLiteral("RGBA"), Qt::CaseInsensitive) != 0) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("ParametricComposition only supports RGBA slot values.");
            }
            return false;
        }
    }
    QMap<QString, bool> seenParameters;
    for (const auto& parameterItem : parameters_) {
        if (parameterItem.key.isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("ParametricComposition parameter key cannot be empty.");
            }
            return false;
        }
        if (seenParameters.contains(parameterItem.key)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Duplicate ParametricComposition parameter key: %1").arg(parameterItem.key);
            }
            return false;
        }
        seenParameters.insert(parameterItem.key, true);
    }
    return true;
}

QJsonObject ParametricCompositionDefinition::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("definitionId"), definitionId_);
    obj.insert(QStringLiteral("displayName"), displayName_);

    QJsonArray slotArray;
    for (const auto& slot : slots_) {
        slotArray.append(slot.toJson());
    }
    obj.insert(QStringLiteral("inputSlots"), slotArray);
    obj.insert(QStringLiteral("slots"), slotArray);

    QJsonArray params;
    for (const auto& parameter : parameters_) {
        params.append(parameter.toJson());
    }
    obj.insert(QStringLiteral("parameters"), params);
    return obj;
}

ParametricCompositionDefinition ParametricCompositionDefinition::fromJson(const QJsonObject& obj)
{
    ParametricCompositionDefinition definition;
    definition.definitionId_ = obj.value(QStringLiteral("definitionId")).toString();
    definition.displayName_ = obj.value(QStringLiteral("displayName")).toString();

    const QJsonArray genericSlots = obj.value(QStringLiteral("slots")).toArray();
    const QJsonArray legacyInputSlots = obj.value(QStringLiteral("inputSlots")).toArray();
    const QJsonArray& sourceSlots = !genericSlots.isEmpty() ? genericSlots : legacyInputSlots;
    for (const auto& slotValue : sourceSlots) {
        if (slotValue.isObject()) {
            definition.slots_.append(ParametricCompositionSlot::fromJson(slotValue.toObject()));
        }
    }

    const QJsonArray params = obj.value(QStringLiteral("parameters")).toArray();
    for (const auto& paramValue : params) {
        if (paramValue.isObject()) {
            definition.parameters_.append(ParametricCompositionParameter::fromJson(paramValue.toObject()));
        }
    }
    return definition;
}

ParametricCompositionInstance::ParametricCompositionInstance(std::shared_ptr<const ParametricCompositionDefinition> definition)
    : definition_(std::move(definition))
{
}

std::shared_ptr<const ParametricCompositionDefinition> ParametricCompositionInstance::definition() const
{
    return definition_;
}

void ParametricCompositionInstance::setDefinition(std::shared_ptr<const ParametricCompositionDefinition> definition)
{
    definition_ = std::move(definition);
}

const ParametricCompositionInputBinding& ParametricCompositionInstance::inputBinding() const
{
    return inputBinding_;
}

void ParametricCompositionInstance::setInputBinding(const ParametricCompositionInputBinding& binding)
{
    inputBinding_ = binding;
}

void ParametricCompositionInstance::clearInputBinding()
{
    inputBinding_ = {};
}

bool ParametricCompositionInstance::isInputConnected() const
{
    return inputBinding_.isConnected();
}

QVariant ParametricCompositionInstance::parameterValue(const QString& key, const QVariant& fallback) const
{
    const auto overrideIt = parameterOverrides_.find(key);
    if (overrideIt != parameterOverrides_.end()) {
        if (definition_) {
            const auto* parameter = definition_->parameter(key);
            if (parameter && !parameter->overridableByInstance) {
                return parameter->defaultValue.isValid() ? parameter->defaultValue : fallback;
            }
        }
        return overrideIt.value();
    }
    if (definition_) {
        const auto* parameter = definition_->parameter(key);
        if (parameter && parameter->defaultValue.isValid()) {
            return parameter->defaultValue;
        }
    }
    return fallback;
}

void ParametricCompositionInstance::setParameterOverride(const QString& key, const QVariant& value)
{
    if (key.isEmpty()) {
        return;
    }
    if (definition_) {
        const auto* parameter = definition_->parameter(key);
        if (parameter && !parameter->overridableByInstance) {
            return;
        }
    }
    parameterOverrides_.insert(key, value);
}

void ParametricCompositionInstance::clearParameterOverride(const QString& key)
{
    parameterOverrides_.remove(key);
}

const QMap<QString, QVariant>& ParametricCompositionInstance::parameterOverrides() const
{
    return parameterOverrides_;
}

QMap<QString, QVariant> ParametricCompositionInstance::resolvedParameters() const
{
    QMap<QString, QVariant> resolved;
    if (definition_) {
        for (const auto& parameter : definition_->parameters()) {
            resolved.insert(parameter.key, parameterValue(parameter.key, parameter.defaultValue));
        }
    }
    for (auto it = parameterOverrides_.cbegin(); it != parameterOverrides_.cend(); ++it) {
        if (!definition_) {
            resolved.insert(it.key(), it.value());
            continue;
        }
        const auto* parameter = definition_->parameter(it.key());
        if (parameter == nullptr || parameter->overridableByInstance) {
            resolved.insert(it.key(), it.value());
        }
    }
    return resolved;
}

QByteArray ParametricCompositionInstance::hashVariantMap(const QMap<QString, QVariant>& values)
{
    QVariantMap variantMap;
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        variantMap.insert(it.key(), it.value());
    }
    return QCryptographicHash::hash(
        QJsonDocument::fromVariant(QVariant(variantMap)).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256
    );
}

QByteArray ParametricCompositionInstance::hashImage(const ImageF32x4_RGBA& image)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    const qint32 width = image.width();
    const qint32 height = image.height();
    hash.addData(reinterpret_cast<const char*>(&width), sizeof(width));
    hash.addData(reinterpret_cast<const char*>(&height), sizeof(height));

    const float* pixels = image.rgba32fData();
    if (pixels != nullptr && width > 0 && height > 0) {
        const qsizetype byteCount = static_cast<qsizetype>(width) * static_cast<qsizetype>(height) * 4 * static_cast<qsizetype>(sizeof(float));
        hash.addData(reinterpret_cast<const char*>(pixels), byteCount);
    }
    return hash.result();
}

QByteArray ParametricCompositionInstance::parameterHash() const
{
    return hashVariantMap(resolvedParameters());
}

QByteArray ParametricCompositionInstance::inputFrameHash(
    const ParametricCompositionRenderContext& context,
    const ParametricCompositionInputResolver& resolver) const
{
    const ParametricCompositionEvaluation evaluation = evaluate(context, resolver);
    return evaluation.cacheKey.inputFrameHash;
}

ParametricCompositionCacheKey ParametricCompositionInstance::cacheKey(
    const ParametricCompositionRenderContext& context,
    const ParametricCompositionInputResolver& resolver) const
{
    const ParametricCompositionEvaluation evaluation = evaluate(context, resolver);
    return evaluation.cacheKey;
}

ImageF32x4_RGBA ParametricCompositionInstance::makeTransparent(const QSize& size)
{
    ImageF32x4_RGBA image(FloatRGBA(0.0f, 0.0f, 0.0f, 0.0f));
    if (size.isValid() && size.width() > 0 && size.height() > 0) {
        image.resize(size.width(), size.height());
    }
    return image;
}

ParametricCompositionEvaluation ParametricCompositionInstance::evaluate(
    const ParametricCompositionRenderContext& context,
    const ParametricCompositionInputResolver& resolver) const
{
    ParametricCompositionEvaluation evaluation;
    const QSize targetSize = context.outputSize.isValid() ? context.outputSize : QSize(1, 1);

    ImageF32x4_RGBA resolvedImage;
    bool resolved = false;
    if (inputBinding_.isConnected()) {
        if (resolver.resolve) {
            const std::optional<ImageF32x4_RGBA> candidate = resolver.resolve(inputBinding_, context);
            if (candidate.has_value() && !candidate->isEmpty()) {
                resolvedImage = *candidate;
                resolved = true;
            }
        }

        if (!resolved) {
            switch (inputBinding_.kind) {
            case ParametricCompositionSlotKind::Image:
                if (!inputBinding_.image.isEmpty()) {
                    resolvedImage = inputBinding_.image;
                    resolved = true;
                }
                break;
            case ParametricCompositionSlotKind::Matte:
                if (!inputBinding_.matte.isEmpty()) {
                    resolvedImage = inputBinding_.matte;
                    resolved = true;
                }
                break;
            case ParametricCompositionSlotKind::Text:
            case ParametricCompositionSlotKind::SourceLayer:
            case ParametricCompositionSlotKind::RGBA:
            case ParametricCompositionSlotKind::Alpha:
            case ParametricCompositionSlotKind::MotionPath:
            case ParametricCompositionSlotKind::Control:
            case ParametricCompositionSlotKind::Event:
                break;
            }
        }
    }

    if (resolved) {
        if (targetSize.isValid() && targetSize.width() > 0 && targetSize.height() > 0 &&
            (resolvedImage.width() != targetSize.width() || resolvedImage.height() != targetSize.height())) {
            resolvedImage.resize(targetSize.width(), targetSize.height());
        }
        evaluation.image = resolvedImage;
    } else {
        evaluation.image = makeTransparent(targetSize);
        evaluation.usedTransparentFallback = true;
    }

    evaluation.inputResolved = resolved;
    evaluation.cacheKey.definitionId = definition_ ? definition_->definitionId() : QString();
    evaluation.cacheKey.inputFrameHash =
        resolved ? hashImage(resolvedImage)
                 : QByteArrayLiteral("transparent-rgba");
    evaluation.cacheKey.parameterHash = parameterHash();
    evaluation.cacheKey.timeKey = context.timeKey();
    return evaluation;
}

ImageF32x4_RGBA ParametricCompositionInstance::render(
    const ParametricCompositionRenderContext& context,
    const ParametricCompositionInputResolver& resolver) const
{
    return evaluate(context, resolver).image;
}

QJsonObject ParametricCompositionInstance::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("inputBinding"), inputBinding_.toJson());

    QJsonObject overrides;
    for (auto it = parameterOverrides_.cbegin(); it != parameterOverrides_.cend(); ++it) {
        overrides.insert(it.key(), jsonValueFromVariant(it.value()));
    }
    obj.insert(QStringLiteral("parameterOverrides"), overrides);
    if (definition_) {
        obj.insert(QStringLiteral("definitionId"), definition_->definitionId());
    }
    return obj;
}

ParametricCompositionInstance ParametricCompositionInstance::fromJson(
    const QJsonObject& obj,
    std::shared_ptr<const ParametricCompositionDefinition> definition)
{
    ParametricCompositionInstance instance(std::move(definition));
    instance.inputBinding_ = ParametricCompositionInputBinding::fromJson(obj.value(QStringLiteral("inputBinding")).toObject());

    const QJsonObject overrides = obj.value(QStringLiteral("parameterOverrides")).toObject();
    for (auto it = overrides.begin(); it != overrides.end(); ++it) {
        instance.parameterOverrides_.insert(it.key(), it.value().toVariant());
    }
    return instance;
}

} // namespace ArtifactCore
