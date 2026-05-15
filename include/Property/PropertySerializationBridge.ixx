module;

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QVariant>
#include <QColor>
#include "../Define/DllExportMacro.hpp"

#include <memory>
#include <vector>
#include <functional>
#include <optional>
#include <utility>

export module Property.SerializationBridge;

import Property.Abstract;
import Property.Group;
import Property.Path;
import Time.Rational;

export namespace ArtifactCore {

struct SerializedProperty {
    QString name;
    int type = 0;
    QJsonValue value;
    QString expression;
    QJsonArray keyframes;
    QJsonObject metadata;
};

struct SerializedOwner {
    PropertyPath ownerPath;
    QString displayName;
    QString ownerType;
    bool readOnly = false;
    std::vector<SerializedProperty> properties;
};

class LIBRARY_DLL_API PropertySerializationBridge {
public:
    using CustomSerializer = std::function<QJsonValue(const AbstractPropertyPtr&)>;
    using CustomDeserializer = std::function<void(AbstractPropertyPtr&, const QJsonValue&)>;

    static QJsonValue serializePropertyValue(const AbstractPropertyPtr& property)
    {
        if (!property) {
            return QJsonValue();
        }

        switch (property->getType()) {
        case PropertyType::Float:
        case PropertyType::Integer:
        case PropertyType::Boolean:
        case PropertyType::String:
            return QJsonValue::fromVariant(property->getValue());
        case PropertyType::Color: {
            QColor c = property->getColorValue();
            QJsonObject col;
            col[QStringLiteral("r")] = c.redF();
            col[QStringLiteral("g")] = c.greenF();
            col[QStringLiteral("b")] = c.blueF();
            col[QStringLiteral("a")] = c.alphaF();
            return col;
        }
        default:
            return QJsonValue::fromVariant(property->getValue());
        }
    }

    static void deserializePropertyValue(const AbstractPropertyPtr& property,
                                          const QJsonValue& value)
    {
        if (!property || value.isUndefined()) {
            return;
        }

        switch (property->getType()) {
        case PropertyType::Color: {
            if (value.isObject()) {
                const QJsonObject col = value.toObject();
                QColor c;
                c.setRedF(static_cast<float>(col.value(QStringLiteral("r")).toDouble(0.0)));
                c.setGreenF(static_cast<float>(col.value(QStringLiteral("g")).toDouble(0.0)));
                c.setBlueF(static_cast<float>(col.value(QStringLiteral("b")).toDouble(0.0)));
                c.setAlphaF(static_cast<float>(col.value(QStringLiteral("a")).toDouble(1.0)));
                property->setColorValue(c);
            }
            return;
        }
        default:
            property->setValue(value.toVariant());
            return;
        }
    }

    static SerializedProperty serializeProperty(const AbstractPropertyPtr& property)
    {
        SerializedProperty sp;
        if (!property) {
            return sp;
        }

        sp.name = property->getName();
        sp.type = static_cast<int>(property->getType());
        sp.value = serializePropertyValue(property);

        if (property->hasExpression()) {
            sp.expression = property->getExpression();
        }

        const auto keyframes = property->getKeyFrames();
        if (!keyframes.empty()) {
            for (const auto& kf : keyframes) {
                QJsonObject kfObj;
                kfObj[QStringLiteral("timeValue")] = kf.time.value();
                kfObj[QStringLiteral("timeScale")] = kf.time.scale();
                kfObj[QStringLiteral("interpolation")] = static_cast<int>(kf.interpolation);
                kfObj[QStringLiteral("cp1_x")] = kf.cp1_x;
                kfObj[QStringLiteral("cp1_y")] = kf.cp1_y;
                kfObj[QStringLiteral("cp2_x")] = kf.cp2_x;
                kfObj[QStringLiteral("cp2_y")] = kf.cp2_y;
                switch (property->getType()) {
                case PropertyType::Float:
                case PropertyType::Integer:
                case PropertyType::Boolean:
                    kfObj[QStringLiteral("value")] = QJsonValue::fromVariant(kf.value);
                    break;
                case PropertyType::Color: {
                    QColor c = kf.value.value<QColor>();
                    QJsonObject col;
                    col[QStringLiteral("r")] = c.redF();
                    col[QStringLiteral("g")] = c.greenF();
                    col[QStringLiteral("b")] = c.blueF();
                    col[QStringLiteral("a")] = c.alphaF();
                    kfObj[QStringLiteral("value")] = col;
                    break;
                }
                default:
                    kfObj[QStringLiteral("value")] = QJsonValue::fromVariant(kf.value);
                    break;
                }
                sp.keyframes.append(kfObj);
            }
        }

        const auto meta = property->metadata();
        if (!meta.displayLabel.isEmpty() || !meta.unit.isEmpty() || !meta.tooltip.isEmpty()) {
            QJsonObject metaObj;
            if (!meta.displayLabel.isEmpty()) {
                metaObj[QStringLiteral("displayLabel")] = meta.displayLabel;
            }
            if (!meta.unit.isEmpty()) {
                metaObj[QStringLiteral("unit")] = meta.unit;
            }
            if (!meta.tooltip.isEmpty()) {
                metaObj[QStringLiteral("tooltip")] = meta.tooltip;
            }
            if (meta.hardMin.isValid()) {
                metaObj[QStringLiteral("hardMin")] = QJsonValue::fromVariant(meta.hardMin);
            }
            if (meta.hardMax.isValid()) {
                metaObj[QStringLiteral("hardMax")] = QJsonValue::fromVariant(meta.hardMax);
            }
            if (meta.softMin.isValid()) {
                metaObj[QStringLiteral("softMin")] = QJsonValue::fromVariant(meta.softMin);
            }
            if (meta.softMax.isValid()) {
                metaObj[QStringLiteral("softMax")] = QJsonValue::fromVariant(meta.softMax);
            }
            sp.metadata = metaObj;
        }

        return sp;
    }

    static void deserializeProperty(AbstractPropertyPtr& property,
                                     const SerializedProperty& sp)
    {
        if (!property) {
            return;
        }

        deserializePropertyValue(property, sp.value);

        if (!sp.expression.isEmpty()) {
            property->setExpression(sp.expression);
        }

        if (!sp.keyframes.isEmpty()) {
            property->clearKeyFrames();
            for (const QJsonValue& kfVal : sp.keyframes) {
                const QJsonObject kfObj = kfVal.toObject();
                RationalTime time(
                    kfObj.value(QStringLiteral("timeValue")).toInteger(),
                    kfObj.value(QStringLiteral("timeScale")).toInteger());
                InterpolationType interpolation = static_cast<InterpolationType>(kfObj.value(QStringLiteral("interpolation")).toInt(static_cast<int>(InterpolationType::Linear)));
                float cp1_x = kfObj.value(QStringLiteral("cp1_x")).toDouble(0.42);
                float cp1_y = kfObj.value(QStringLiteral("cp1_y")).toDouble(0.0);
                float cp2_x = kfObj.value(QStringLiteral("cp2_x")).toDouble(0.58);
                float cp2_y = kfObj.value(QStringLiteral("cp2_y")).toDouble(1.0);
                const QJsonValue val = kfObj.value(QStringLiteral("value"));
                property->addKeyFrame(time, val.toVariant(), interpolation, cp1_x, cp1_y, cp2_x, cp2_y);
            }
        }
    }

    static SerializedOwner serializeOwner(const PropertyPath& ownerPath,
                                           const QString& displayName,
                                           const QString& ownerType,
                                           bool readOnly,
                                           const std::shared_ptr<PropertyGroup>& group)
    {
        SerializedOwner so;
        so.ownerPath = ownerPath;
        so.displayName = displayName;
        so.ownerType = ownerType;
        so.readOnly = readOnly;

        if (group) {
            const auto props = group->allProperties();
            so.properties.reserve(props.size());
            for (const auto& p : props) {
                so.properties.push_back(serializeProperty(p));
            }
        }

        return so;
    }

    static QJsonObject serializedOwnerToJson(const SerializedOwner& so)
    {
        QJsonObject obj;
        obj[QStringLiteral("ownerPath")] = so.ownerPath.toString();
        obj[QStringLiteral("displayName")] = so.displayName;
        obj[QStringLiteral("ownerType")] = so.ownerType;
        obj[QStringLiteral("readOnly")] = so.readOnly;

        QJsonArray propsArr;
        for (const auto& sp : so.properties) {
            QJsonObject pobj;
            pobj[QStringLiteral("name")] = sp.name;
            pobj[QStringLiteral("type")] = sp.type;
            pobj[QStringLiteral("value")] = sp.value;
            if (!sp.expression.isEmpty()) {
                pobj[QStringLiteral("expression")] = sp.expression;
            }
            if (!sp.keyframes.isEmpty()) {
                pobj[QStringLiteral("keyframes")] = sp.keyframes;
            }
            if (!sp.metadata.isEmpty()) {
                pobj[QStringLiteral("metadata")] = sp.metadata;
            }
            propsArr.append(pobj);
        }
        obj[QStringLiteral("properties")] = propsArr;

        return obj;
    }

    static SerializedOwner serializedOwnerFromJson(const QJsonObject& obj)
    {
        SerializedOwner so;
        so.ownerPath = PropertyPath(obj.value(QStringLiteral("ownerPath")).toString());
        so.displayName = obj.value(QStringLiteral("displayName")).toString();
        so.ownerType = obj.value(QStringLiteral("ownerType")).toString();
        so.readOnly = obj.value(QStringLiteral("readOnly")).toBool(false);

        const QJsonArray propsArr = obj.value(QStringLiteral("properties")).toArray();
        so.properties.reserve(propsArr.size());
        for (const QJsonValue& pv : propsArr) {
            const QJsonObject pobj = pv.toObject();
            SerializedProperty sp;
            sp.name = pobj.value(QStringLiteral("name")).toString();
            sp.type = pobj.value(QStringLiteral("type")).toInt();
            sp.value = pobj.value(QStringLiteral("value"));
            sp.expression = pobj.value(QStringLiteral("expression")).toString();
            sp.keyframes = pobj.value(QStringLiteral("keyframes")).toArray();
            sp.metadata = pobj.value(QStringLiteral("metadata")).toObject();
            so.properties.push_back(std::move(sp));
        }

        return so;
    }
};

} // namespace ArtifactCore
