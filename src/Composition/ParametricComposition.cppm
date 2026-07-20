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

void addLengthPrefixedInputHash(QCryptographicHash& hash,
                                const QString& inputId,
                                const QByteArray& frameHash)
{
    const QByteArray encodedInputId = inputId.toUtf8();
    const quint64 inputIdSize = static_cast<quint64>(encodedInputId.size());
    const quint64 frameHashSize = static_cast<quint64>(frameHash.size());
    hash.addData(reinterpret_cast<const char*>(&inputIdSize), sizeof(inputIdSize));
    hash.addData(encodedInputId);
    hash.addData(reinterpret_cast<const char*>(&frameHashSize), sizeof(frameHashSize));
    hash.addData(frameHash);
}

QJsonArray templateSlotsObjectToArray(const QJsonObject& templateSlots)
{
    QJsonArray slotArray;
    for (auto it = templateSlots.begin(); it != templateSlots.end(); ++it) {
        if (!it.value().isObject()) {
            continue;
        }
        QJsonObject slot = it.value().toObject();
        if (!slot.contains(QStringLiteral("slotId"))) {
            slot.insert(QStringLiteral("slotId"), it.key());
        }
        if (!slot.contains(QStringLiteral("displayName"))) {
            slot.insert(QStringLiteral("displayName"), it.key());
        }
        if (!slot.contains(QStringLiteral("role"))) {
            slot.insert(QStringLiteral("role"), static_cast<int>(ParametricCompositionSlotRole::Input));
        }
        if (!slot.contains(QStringLiteral("kind"))) {
            slot.insert(QStringLiteral("kind"), static_cast<int>(ParametricCompositionSlotKind::SourceLayer));
        }
        if (!slot.contains(QStringLiteral("valueType"))) {
            slot.insert(QStringLiteral("valueType"), QStringLiteral("RGBA"));
        }
        slotArray.append(slot);
    }
    return slotArray;
}

QJsonArray templateSlotsToValuesArray(const QJsonObject& templateSlots)
{
    QJsonArray values;
    for (auto it = templateSlots.begin(); it != templateSlots.end(); ++it) {
        if (!it.value().isObject()) {
            continue;
        }
        const QJsonObject slot = it.value().toObject();
        QJsonObject entry;
        entry.insert(QStringLiteral("slotId"), slot.value(QStringLiteral("slotId")).toString(it.key()));
        entry.insert(QStringLiteral("slotName"), slot.value(QStringLiteral("displayName")).toString(it.key()));
        entry.insert(QStringLiteral("value"), slot.value(QStringLiteral("defaultValue")));
        values.append(entry);
    }
    return values;
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

QJsonObject ParametricCompositionPublishedControl::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("controlId"), controlId);
    obj.insert(QStringLiteral("sourceParameterKey"), sourceParameterKey);
    obj.insert(QStringLiteral("displayName"), displayName);
    obj.insert(QStringLiteral("group"), group);
    obj.insert(QStringLiteral("order"), order);
    obj.insert(QStringLiteral("valueType"), valueType);
    obj.insert(QStringLiteral("defaultValue"), jsonValueFromVariant(defaultValue));
    obj.insert(QStringLiteral("minimumValue"), jsonValueFromVariant(minimumValue));
    obj.insert(QStringLiteral("maximumValue"), jsonValueFromVariant(maximumValue));
    obj.insert(QStringLiteral("enumChoices"), QJsonArray::fromStringList(enumChoices));
    obj.insert(QStringLiteral("readOnly"), readOnly);
    obj.insert(QStringLiteral("hidden"), hidden);
    obj.insert(QStringLiteral("required"), required);
    return obj;
}

ParametricCompositionPublishedControl ParametricCompositionPublishedControl::fromJson(const QJsonObject& obj)
{
    ParametricCompositionPublishedControl control;
    control.controlId = obj.value(QStringLiteral("controlId")).toString();
    control.sourceParameterKey = obj.value(QStringLiteral("sourceParameterKey")).toString();
    control.displayName = obj.value(QStringLiteral("displayName")).toString();
    control.group = obj.value(QStringLiteral("group")).toString();
    control.order = obj.value(QStringLiteral("order")).toInt(0);
    control.valueType = obj.value(QStringLiteral("valueType")).toString();
    control.defaultValue = variantFromJsonValue(obj.value(QStringLiteral("defaultValue")));
    control.minimumValue = variantFromJsonValue(obj.value(QStringLiteral("minimumValue")));
    control.maximumValue = variantFromJsonValue(obj.value(QStringLiteral("maximumValue")));
    const QJsonArray enumChoices = obj.value(QStringLiteral("enumChoices")).toArray();
    for (const auto& value : enumChoices) {
        control.enumChoices.append(value.toString());
    }
    control.readOnly = obj.value(QStringLiteral("readOnly")).toBool(false);
    control.hidden = obj.value(QStringLiteral("hidden")).toBool(false);
    control.required = obj.value(QStringLiteral("required")).toBool(false);
    return control;
}

QJsonObject ParametricCompositionDataBinding::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("columnKey"), columnKey);
    obj.insert(QStringLiteral("targetParameterKey"), targetParameterKey);
    obj.insert(QStringLiteral("targetPublishedControlId"), targetPublishedControlId);
    obj.insert(QStringLiteral("fallbackValue"), jsonValueFromVariant(fallbackValue));
    obj.insert(QStringLiteral("enabled"), enabled);
    return obj;
}

ParametricCompositionDataBinding ParametricCompositionDataBinding::fromJson(const QJsonObject& obj)
{
    ParametricCompositionDataBinding binding;
    binding.columnKey = obj.value(QStringLiteral("columnKey")).toString();
    binding.targetParameterKey = obj.value(QStringLiteral("targetParameterKey")).toString();
    binding.targetPublishedControlId = obj.value(QStringLiteral("targetPublishedControlId")).toString();
    binding.fallbackValue = variantFromJsonValue(obj.value(QStringLiteral("fallbackValue")));
    binding.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
    return binding;
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
    QCryptographicHash combinedHash(QCryptographicHash::Sha256);
    for (auto it = inputFrameHashes.cbegin(); it != inputFrameHashes.cend(); ++it) {
        // Encode boundaries explicitly.  Concatenating raw key/value bytes
        // allows distinct input maps to produce the same digest stream.
        addLengthPrefixedInputHash(combinedHash, it.key(), it.value());
    }
    // Bump the cache-key schema whenever the serialized hash stream changes.
    // This prevents stale entries created by older clients from being reused.
    return QStringLiteral("pcache-v2|%1|%2|%3|%4")
        .arg(definitionId)
        .arg(QString::fromLatin1(combinedHash.result().toHex()))
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
    if (!definition.contains(QStringLiteral("slots")) &&
        definition.contains(QStringLiteral("templateSlots"))) {
        obj.insert(QStringLiteral("templateSlots"), definition.value(QStringLiteral("templateSlots")));
    }
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
    if (bundle.definition.isEmpty() && obj.contains(QStringLiteral("templateSlots"))) {
        const QJsonObject templateSlots = obj.value(QStringLiteral("templateSlots")).toObject();
        if (!templateSlots.isEmpty()) {
            bundle.definition.insert(QStringLiteral("slots"), templateSlotsObjectToArray(templateSlots));
            bundle.definition.insert(QStringLiteral("templateSlots"), templateSlots);
            if (!bundle.instance.contains(QStringLiteral("slotValues"))) {
                bundle.instance.insert(QStringLiteral("slotValues"), templateSlotsToValuesArray(templateSlots));
            }
        }
    }
    if (bundle.instance.isEmpty() && obj.contains(QStringLiteral("slotValues"))) {
        bundle.instance.insert(QStringLiteral("slotValues"), obj.value(QStringLiteral("slotValues")));
    }
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
    if (!compositionJson.contains(QStringLiteral("slots")) &&
        compositionJson.contains(QStringLiteral("templateSlots"))) {
        const QJsonObject templateSlots = compositionJson.value(QStringLiteral("templateSlots")).toObject();
        compositionJson.insert(QStringLiteral("slots"), templateSlotsObjectToArray(templateSlots));
        if (!compositionJson.contains(QStringLiteral("slotValues"))) {
            compositionJson.insert(QStringLiteral("slotValues"), templateSlotsToValuesArray(templateSlots));
        }
    }
    if (!bundle.bundleTitle.isEmpty()) {
        compositionJson.insert(QStringLiteral("displayName"), bundle.bundleTitle);
    }
    compositionJson.insert(QStringLiteral("bundleKind"), bundle.bundleKind);
    compositionJson.insert(QStringLiteral("bundleMetadata"), bundle.metadata);
    if (!bundle.instance.isEmpty()) {
        compositionJson.insert(QStringLiteral("instance"), bundle.instance);
    }
    if (bundle.instance.contains(QStringLiteral("slotValues")) &&
        !compositionJson.contains(QStringLiteral("slotValues"))) {
        compositionJson.insert(QStringLiteral("slotValues"), bundle.instance.value(QStringLiteral("slotValues")));
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

QVector<ParametricCompositionSlot> ParametricCompositionDefinition::inputSlots() const
{
    return slotsByRole(ParametricCompositionSlotRole::Input);
}

QVector<ParametricCompositionSlot> ParametricCompositionDefinition::outputSlots() const
{
    return slotsByRole(ParametricCompositionSlotRole::Output);
}

const ParametricCompositionSlot* ParametricCompositionDefinition::slot(const QString& slotId) const
{
    for (const auto& item : slots_) {
        if (item.slotId == slotId) {
            return &item;
        }
    }
    return nullptr;
}

bool ParametricCompositionDefinition::hasSlot(const QString& slotId) const
{
    return slot(slotId) != nullptr;
}

const QVector<ParametricCompositionParameter>& ParametricCompositionDefinition::parameters() const
{
    return parameters_;
}

const QVector<ParametricCompositionPublishedControl>& ParametricCompositionDefinition::publishedControls() const
{
    return publishedControls_;
}

const QVector<ParametricCompositionDataBinding>& ParametricCompositionDefinition::dataBindings() const
{
    return dataBindings_;
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
    if (slot.slotId.isEmpty()) {
        return false;
    }
    for (auto& existing : slots_) {
        if (existing.slotId == slot.slotId) {
            existing = slot;
            return true;
        }
    }
    slots_.append(slot);
    return true;
}

bool ParametricCompositionDefinition::removeSlot(const QString& slotId)
{
    for (auto it = slots_.begin(); it != slots_.end(); ++it) {
        if (it->slotId == slotId) {
            slots_.erase(it);
            return true;
        }
    }
    return false;
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

bool ParametricCompositionDefinition::addPublishedControl(const ParametricCompositionPublishedControl& control)
{
    if (control.controlId.isEmpty() || control.sourceParameterKey.isEmpty()) {
        return false;
    }
    for (const auto& existing : publishedControls_) {
        if (existing.controlId == control.controlId) {
            return false;
        }
    }
    publishedControls_.append(control);
    return true;
}

bool ParametricCompositionDefinition::setPublishedControl(const ParametricCompositionPublishedControl& control)
{
    if (control.controlId.isEmpty() || control.sourceParameterKey.isEmpty()) {
        return false;
    }
    for (auto& existing : publishedControls_) {
        if (existing.controlId == control.controlId) {
            existing = control;
            return true;
        }
    }
    publishedControls_.append(control);
    return true;
}

bool ParametricCompositionDefinition::removePublishedControl(const QString& controlId)
{
    for (auto it = publishedControls_.begin(); it != publishedControls_.end(); ++it) {
        if (it->controlId == controlId) {
            publishedControls_.erase(it);
            return true;
        }
    }
    return false;
}

bool ParametricCompositionDefinition::hasPublishedControl(const QString& controlId) const
{
    return publishedControl(controlId) != nullptr;
}

const ParametricCompositionPublishedControl* ParametricCompositionDefinition::publishedControl(const QString& controlId) const
{
    for (const auto& item : publishedControls_) {
        if (item.controlId == controlId) {
            return &item;
        }
    }
    return nullptr;
}

bool ParametricCompositionDefinition::addDataBinding(const ParametricCompositionDataBinding& binding)
{
    if (binding.columnKey.isEmpty()) {
        return false;
    }
    for (const auto& existing : dataBindings_) {
        if (existing.columnKey == binding.columnKey) {
            return false;
        }
    }
    dataBindings_.append(binding);
    return true;
}

bool ParametricCompositionDefinition::setDataBinding(const ParametricCompositionDataBinding& binding)
{
    if (binding.columnKey.isEmpty()) {
        return false;
    }
    for (auto& existing : dataBindings_) {
        if (existing.columnKey == binding.columnKey) {
            existing = binding;
            return true;
        }
    }
    dataBindings_.append(binding);
    return true;
}

bool ParametricCompositionDefinition::removeDataBinding(const QString& columnKey)
{
    for (auto it = dataBindings_.begin(); it != dataBindings_.end(); ++it) {
        if (it->columnKey == columnKey) {
            dataBindings_.erase(it);
            return true;
        }
    }
    return false;
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
    if (inputOnlySlots.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("ParametricComposition requires at least one input slot.");
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
        if (slot.slotId.isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("ParametricComposition slotId cannot be empty.");
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
    QMap<QString, bool> seenControls;
    for (const auto& controlItem : publishedControls_) {
        if (controlItem.controlId.isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("ParametricComposition published controlId cannot be empty.");
            }
            return false;
        }
        if (controlItem.sourceParameterKey.isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Published control sourceParameterKey cannot be empty.");
            }
            return false;
        }
        if (seenControls.contains(controlItem.controlId)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Duplicate ParametricComposition published controlId: %1").arg(controlItem.controlId);
            }
            return false;
        }
        if (!seenParameters.contains(controlItem.sourceParameterKey)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Published control sourceParameterKey not found: %1").arg(controlItem.sourceParameterKey);
            }
            return false;
        }
        seenControls.insert(controlItem.controlId, true);
    }
    for (const auto& binding : dataBindings_) {
        if (binding.columnKey.isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("ParametricComposition data binding columnKey cannot be empty.");
            }
            return false;
        }
        if (binding.targetParameterKey.isEmpty() && binding.targetPublishedControlId.isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("ParametricComposition data binding needs a target.");
            }
            return false;
        }
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

    QJsonArray publishedControls;
    for (const auto& control : publishedControls_) {
        publishedControls.append(control.toJson());
    }
    obj.insert(QStringLiteral("publishedControls"), publishedControls);
    QJsonArray dataBindings;
    for (const auto& binding : dataBindings_) {
        dataBindings.append(binding.toJson());
    }
    obj.insert(QStringLiteral("dataBindings"), dataBindings);
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

    const QJsonArray publishedControls = obj.value(QStringLiteral("publishedControls")).toArray();
    for (const auto& controlValue : publishedControls) {
        if (controlValue.isObject()) {
            definition.publishedControls_.append(ParametricCompositionPublishedControl::fromJson(controlValue.toObject()));
        }
    }
    const QJsonArray dataBindings = obj.value(QStringLiteral("dataBindings")).toArray();
    for (const auto& bindingValue : dataBindings) {
        if (bindingValue.isObject()) {
            definition.dataBindings_.append(ParametricCompositionDataBinding::fromJson(bindingValue.toObject()));
        }
    }

    if (definition.slots_.isEmpty() && obj.contains(QStringLiteral("templateSlots"))) {
        const QJsonObject templateSlots = obj.value(QStringLiteral("templateSlots")).toObject();
        const auto slotsArray = templateSlotsObjectToArray(templateSlots);
        for (const auto& slotValue : slotsArray) {
            if (slotValue.isObject()) {
                definition.slots_.append(ParametricCompositionSlot::fromJson(slotValue.toObject()));
            }
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

const QVector<ParametricCompositionInputBinding>& ParametricCompositionInstance::inputBindings() const
{
    return inputBindings_;
}

void ParametricCompositionInstance::addInputBinding(const ParametricCompositionInputBinding& binding)
{
    inputBindings_.append(binding);
}

void ParametricCompositionInstance::setInputBinding(int index, const ParametricCompositionInputBinding& binding)
{
    if (index >= 0 && index < inputBindings_.size()) {
        inputBindings_[index] = binding;
    }
}

void ParametricCompositionInstance::removeInputBinding(int index)
{
    if (index >= 0 && index < inputBindings_.size()) {
        inputBindings_.removeAt(index);
    }
}

void ParametricCompositionInstance::clearInputBindings()
{
    inputBindings_.clear();
}

int ParametricCompositionInstance::inputBindingCount() const
{
    return inputBindings_.size();
}

bool ParametricCompositionInstance::isInputConnected(int index) const
{
    if (index >= 0 && index < inputBindings_.size()) {
        return inputBindings_[index].isConnected();
    }
    return false;
}

bool ParametricCompositionInstance::hasAnyInputConnected() const
{
    for (const auto& b : inputBindings_) {
        if (b.isConnected()) return true;
    }
    return false;
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

QVariant ParametricCompositionInstance::publishedControlValue(const QString& controlId, const QVariant& fallback) const
{
    if (!definition_) {
        return fallback;
    }
    const auto* control = definition_->publishedControl(controlId);
    if (!control) {
        return fallback;
    }
    const QVariant controlFallback = control->defaultValue.isValid() ? control->defaultValue : fallback;
    return parameterValue(control->sourceParameterKey, controlFallback);
}

void ParametricCompositionInstance::setPublishedControlOverride(const QString& controlId, const QVariant& value)
{
    if (!definition_) {
        return;
    }
    const auto* control = definition_->publishedControl(controlId);
    if (!control || control->readOnly) {
        return;
    }
    setParameterOverride(control->sourceParameterKey, value);
}

void ParametricCompositionInstance::clearPublishedControlOverride(const QString& controlId)
{
    if (!definition_) {
        return;
    }
    const auto* control = definition_->publishedControl(controlId);
    if (!control) {
        return;
    }
    clearParameterOverride(control->sourceParameterKey);
}

void ParametricCompositionInstance::applyDataRow(const QVariantMap& rowValues)
{
    dataRowValues_ = rowValues;
    if (!definition_) {
        return;
    }

    for (const auto& binding : definition_->dataBindings()) {
        if (!binding.enabled || binding.columnKey.isEmpty()) {
            continue;
        }
        const QVariant value =
            rowValues.contains(binding.columnKey)
                ? rowValues.value(binding.columnKey)
                : binding.fallbackValue;

        if (!binding.targetPublishedControlId.isEmpty()) {
            setPublishedControlOverride(binding.targetPublishedControlId, value);
        } else if (!binding.targetParameterKey.isEmpty()) {
            setParameterOverride(binding.targetParameterKey, value);
        }
    }
}

QVariantMap ParametricCompositionInstance::dataRowValues() const
{
    return dataRowValues_;
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
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (auto it = evaluation.cacheKey.inputFrameHashes.cbegin(); it != evaluation.cacheKey.inputFrameHashes.cend(); ++it) {
        addLengthPrefixedInputHash(hash, it.key(), it.value());
    }
    return hash.result();
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

    bool anyResolved = false;

    for (const auto& inputBinding : inputBindings_) {
        if (!inputBinding.isConnected()) {
            continue;
        }
        if (inputBinding.kind == ParametricCompositionSlotKind::Bool) {
            continue;
        }

        ImageF32x4_RGBA resolved;

        if (resolver.resolve) {
            const std::optional<ImageF32x4_RGBA> candidate = resolver.resolve(inputBinding, context);
            if (candidate.has_value() && !candidate->isEmpty()) {
                resolved = *candidate;
            }
        }

        if (resolved.isEmpty()) {
            switch (inputBinding.kind) {
            case ParametricCompositionSlotKind::Image:
                if (!inputBinding.image.isEmpty()) {
                    resolved = inputBinding.image;
                }
                break;
            case ParametricCompositionSlotKind::Matte:
                if (!inputBinding.matte.isEmpty()) {
                    resolved = inputBinding.matte;
                }
                break;
            case ParametricCompositionSlotKind::Text:
            case ParametricCompositionSlotKind::SourceLayer:
            case ParametricCompositionSlotKind::RGBA:
            case ParametricCompositionSlotKind::Alpha:
            case ParametricCompositionSlotKind::MotionPath:
            case ParametricCompositionSlotKind::Bool:
            case ParametricCompositionSlotKind::Control:
            case ParametricCompositionSlotKind::Event:
                break;
            }
        }

        if (!resolved.isEmpty()) {
            if (targetSize.isValid() && targetSize.width() > 0 && targetSize.height() > 0 &&
                (resolved.width() != targetSize.width() || resolved.height() != targetSize.height())) {
                resolved.resize(targetSize.width(), targetSize.height());
            }
            evaluation.resolvedInputs.insert(inputBinding.slotId, resolved);
            anyResolved = true;
        }
    }

    if (anyResolved) {
        evaluation.image = evaluation.resolvedInputs.cbegin().value();
    } else {
        evaluation.image = makeTransparent(targetSize);
        evaluation.usedTransparentFallback = true;
    }

    evaluation.inputResolved = anyResolved;
    evaluation.cacheKey.definitionId = definition_ ? definition_->definitionId() : QString();

    for (auto it = evaluation.resolvedInputs.cbegin(); it != evaluation.resolvedInputs.cend(); ++it) {
        evaluation.cacheKey.inputFrameHashes.insert(it.key(), hashImage(it.value()));
    }
    if (evaluation.cacheKey.inputFrameHashes.isEmpty()) {
        evaluation.cacheKey.inputFrameHashes.insert(QStringLiteral("_transparent"), QByteArrayLiteral("transparent-rgba"));
    }

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
    QJsonArray bindingArray;
    for (const auto& b : inputBindings_) {
        bindingArray.append(b.toJson());
    }
    obj.insert(QStringLiteral("inputBindings"), bindingArray);

    QJsonObject overrides;
    for (auto it = parameterOverrides_.cbegin(); it != parameterOverrides_.cend(); ++it) {
        overrides.insert(it.key(), jsonValueFromVariant(it.value()));
    }
    obj.insert(QStringLiteral("parameterOverrides"), overrides);
    obj.insert(QStringLiteral("dataRowValues"), QJsonObject::fromVariantMap(dataRowValues_));
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
    const QJsonArray bindingArray = obj.value(QStringLiteral("inputBindings")).toArray();
    for (const auto& bv : bindingArray) {
        instance.inputBindings_.append(
            ParametricCompositionInputBinding::fromJson(bv.toObject()));
    }
    if (instance.inputBindings_.isEmpty() && obj.contains(QStringLiteral("inputBinding"))) {
        instance.inputBindings_.append(
            ParametricCompositionInputBinding::fromJson(obj.value(QStringLiteral("inputBinding")).toObject()));
    }

    const QJsonObject overrides = obj.value(QStringLiteral("parameterOverrides")).toObject();
    for (auto it = overrides.begin(); it != overrides.end(); ++it) {
        instance.parameterOverrides_.insert(it.key(), it.value().toVariant());
    }
    instance.dataRowValues_ = obj.value(QStringLiteral("dataRowValues")).toObject().toVariantMap();
    return instance;
}

} // namespace ArtifactCore
