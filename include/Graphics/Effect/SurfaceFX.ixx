module;
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QUuid>
#include "../../Define/DllExportMacro.hpp"

export module Graphics.Effect.SurfaceFX;

import Serialization.JsonAdapter;
import Serialization.SchemaMigration;

export namespace ArtifactCore {

enum class SurfaceFXAnchorType {
    ScreenSpace,
    Planar,
    TrackedPlanar,
    WorldSurface
};

enum class SurfaceFXElementType {
    Scratch,
    Droplet,
    Streak,
    Condensation,
    Dirt,
    TextureDecal
};

struct LIBRARY_DLL_API SurfaceFXElement {
    QString id;
    SurfaceFXElementType type = SurfaceFXElementType::Scratch;
    float x = 0.5f;
    float y = 0.5f;
    float width = 0.1f;
    float height = 0.02f;
    float rotation = 0.0f;
    float intensity = 1.0f;
    float opacity = 1.0f;
    float roughness = 0.5f;
    int seedOffset = 0;
    float inTime = 0.0f;
    float outTime = -1.0f;
    QString texturePath;
    QUuid assetId;
    QString blendMode = QStringLiteral("normal");
    float tintR = 1.0f;
    float tintG = 1.0f;
    float tintB = 1.0f;
    float tintA = 1.0f;
    float pivotX = 0.5f;
    float pivotY = 0.5f;
    float fadeIn = 0.0f;
    float fadeOut = 0.0f;
    float growth = 0.0f;

    QJsonObject toJson() const {
        QJsonObject json;
        json.insert(QStringLiteral("id"), id);
        json.insert(QStringLiteral("type"), typeName(type));
        json.insert(QStringLiteral("x"), x);
        json.insert(QStringLiteral("y"), y);
        json.insert(QStringLiteral("width"), width);
        json.insert(QStringLiteral("height"), height);
        json.insert(QStringLiteral("rotation"), rotation);
        json.insert(QStringLiteral("intensity"), intensity);
        json.insert(QStringLiteral("opacity"), opacity);
        json.insert(QStringLiteral("roughness"), roughness);
        json.insert(QStringLiteral("seedOffset"), seedOffset);
        json.insert(QStringLiteral("inTime"), inTime);
        json.insert(QStringLiteral("outTime"), outTime);
        json.insert(QStringLiteral("texturePath"), texturePath);
        json.insert(QStringLiteral("assetId"), assetId.toString(QUuid::WithoutBraces));
        json.insert(QStringLiteral("blendMode"), blendMode);
        json.insert(QStringLiteral("tintR"), tintR);
        json.insert(QStringLiteral("tintG"), tintG);
        json.insert(QStringLiteral("tintB"), tintB);
        json.insert(QStringLiteral("tintA"), tintA);
        json.insert(QStringLiteral("pivotX"), pivotX);
        json.insert(QStringLiteral("pivotY"), pivotY);
        json.insert(QStringLiteral("fadeIn"), fadeIn);
        json.insert(QStringLiteral("fadeOut"), fadeOut);
        json.insert(QStringLiteral("growth"), growth);
        return json;
    }

    static SurfaceFXElement fromJson(const QJsonObject& json) {
        SurfaceFXElement result;
        result.id = json.value(QStringLiteral("id")).toString();
        result.type = typeFromName(json.value(QStringLiteral("type")).toString());
        result.x = std::clamp(static_cast<float>(json.value(QStringLiteral("x")).toDouble(result.x)), 0.0f, 1.0f);
        result.y = std::clamp(static_cast<float>(json.value(QStringLiteral("y")).toDouble(result.y)), 0.0f, 1.0f);
        result.width = std::clamp(static_cast<float>(json.value(QStringLiteral("width")).toDouble(result.width)), 0.0f, 1.0f);
        result.height = std::clamp(static_cast<float>(json.value(QStringLiteral("height")).toDouble(result.height)), 0.0f, 1.0f);
        result.rotation = static_cast<float>(json.value(QStringLiteral("rotation")).toDouble(result.rotation));
        result.intensity = std::clamp(static_cast<float>(json.value(QStringLiteral("intensity")).toDouble(result.intensity)), 0.0f, 4.0f);
        result.opacity = std::clamp(static_cast<float>(json.value(QStringLiteral("opacity")).toDouble(result.opacity)), 0.0f, 1.0f);
        result.roughness = std::clamp(static_cast<float>(json.value(QStringLiteral("roughness")).toDouble(result.roughness)), 0.0f, 1.0f);
        result.seedOffset = json.value(QStringLiteral("seedOffset")).toInt(result.seedOffset);
        result.inTime = std::max(0.0f, static_cast<float>(json.value(QStringLiteral("inTime")).toDouble(result.inTime)));
        result.outTime = static_cast<float>(json.value(QStringLiteral("outTime")).toDouble(result.outTime));
        result.texturePath = json.value(QStringLiteral("texturePath")).toString().trimmed().left(32768);
        result.assetId = QUuid::fromString(json.value(QStringLiteral("assetId")).toString());
        result.blendMode = normalizedBlendMode(json.value(QStringLiteral("blendMode")).toString(result.blendMode));
        result.tintR = unitValue(json, QStringLiteral("tintR"), result.tintR);
        result.tintG = unitValue(json, QStringLiteral("tintG"), result.tintG);
        result.tintB = unitValue(json, QStringLiteral("tintB"), result.tintB);
        result.tintA = unitValue(json, QStringLiteral("tintA"), result.tintA);
        result.pivotX = unitValue(json, QStringLiteral("pivotX"), result.pivotX);
        result.pivotY = unitValue(json, QStringLiteral("pivotY"), result.pivotY);
        result.fadeIn = nonNegativeValue(json, QStringLiteral("fadeIn"), result.fadeIn);
        result.fadeOut = nonNegativeValue(json, QStringLiteral("fadeOut"), result.fadeOut);
        const float growthValue = static_cast<float>(
            json.value(QStringLiteral("growth")).toDouble(result.growth));
        result.growth = std::isfinite(growthValue)
            ? std::clamp(growthValue, -1.0f, 4.0f)
            : 0.0f;
        return result;
    }

private:
    static QString typeName(SurfaceFXElementType type) {
        switch (type) {
        case SurfaceFXElementType::TextureDecal: return QStringLiteral("textureDecal");
        case SurfaceFXElementType::Droplet: return QStringLiteral("droplet");
        case SurfaceFXElementType::Streak: return QStringLiteral("streak");
        case SurfaceFXElementType::Condensation: return QStringLiteral("condensation");
        case SurfaceFXElementType::Dirt: return QStringLiteral("dirt");
        case SurfaceFXElementType::Scratch: default: return QStringLiteral("scratch");
        }
    }

    static SurfaceFXElementType typeFromName(const QString& name) {
        if (name == QStringLiteral("textureDecal")) return SurfaceFXElementType::TextureDecal;
        if (name == QStringLiteral("droplet")) return SurfaceFXElementType::Droplet;
        if (name == QStringLiteral("streak")) return SurfaceFXElementType::Streak;
        if (name == QStringLiteral("condensation")) return SurfaceFXElementType::Condensation;
        if (name == QStringLiteral("dirt")) return SurfaceFXElementType::Dirt;
        return SurfaceFXElementType::Scratch;
    }

    static float unitValue(const QJsonObject& json, const QString& key, float fallback) {
        const float value = static_cast<float>(json.value(key).toDouble(fallback));
        return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : fallback;
    }

    static float nonNegativeValue(const QJsonObject& json, const QString& key, float fallback) {
        const float value = static_cast<float>(json.value(key).toDouble(fallback));
        return std::isfinite(value) ? std::clamp(value, 0.0f, 3600.0f) : fallback;
    }

    static QString normalizedBlendMode(const QString& value) {
        const QString normalized = value.trimmed().toLower();
        if (normalized == QStringLiteral("multiply") ||
            normalized == QStringLiteral("screen") ||
            normalized == QStringLiteral("add") ||
            normalized == QStringLiteral("darken"))
            return normalized;
        return QStringLiteral("normal");
    }
};

struct LIBRARY_DLL_API SurfaceFXData {
    SurfaceFXAnchorType anchorType = SurfaceFXAnchorType::ScreenSpace;
    float anchorX = 0.0f;
    float anchorY = 0.0f;
    float anchorWidth = 1.0f;
    float anchorHeight = 1.0f;
    float feather = 0.0f;
    int fieldSeed = 1;
    std::vector<SurfaceFXElement> elements;

    QJsonObject toJson() const {
        QJsonObject json;
        json.insert(QStringLiteral("version"), 3);
        json.insert(QStringLiteral("anchorType"), anchorName(anchorType));
        json.insert(QStringLiteral("anchorX"), anchorX);
        json.insert(QStringLiteral("anchorY"), anchorY);
        json.insert(QStringLiteral("anchorWidth"), anchorWidth);
        json.insert(QStringLiteral("anchorHeight"), anchorHeight);
        json.insert(QStringLiteral("feather"), feather);
        json.insert(QStringLiteral("fieldSeed"), fieldSeed);
        QJsonArray elementsJson;
        for (const auto& element : elements) elementsJson.append(element.toJson());
        json.insert(QStringLiteral("elements"), elementsJson);
        return json;
    }

    static SurfaceFXData fromJson(const QJsonObject& json) {
        SurfaceFXData result;
        result.anchorType = anchorFromName(json.value(QStringLiteral("anchorType")).toString());
        result.anchorX = std::clamp(static_cast<float>(json.value(QStringLiteral("anchorX")).toDouble(result.anchorX)), 0.0f, 1.0f);
        result.anchorY = std::clamp(static_cast<float>(json.value(QStringLiteral("anchorY")).toDouble(result.anchorY)), 0.0f, 1.0f);
        result.anchorWidth = std::clamp(static_cast<float>(json.value(QStringLiteral("anchorWidth")).toDouble(result.anchorWidth)), 0.0f, 1.0f);
        result.anchorHeight = std::clamp(static_cast<float>(json.value(QStringLiteral("anchorHeight")).toDouble(result.anchorHeight)), 0.0f, 1.0f);
        result.feather = std::clamp(static_cast<float>(json.value(QStringLiteral("feather")).toDouble(result.feather)), 0.0f, 1.0f);
        result.fieldSeed = json.value(QStringLiteral("fieldSeed")).toInt(result.fieldSeed);
        result.anchorWidth = std::min(result.anchorWidth, 1.0f - result.anchorX);
        result.anchorHeight = std::min(result.anchorHeight, 1.0f - result.anchorY);
        for (const auto& value : json.value(QStringLiteral("elements")).toArray())
            result.elements.push_back(SurfaceFXElement::fromJson(value.toObject()));
        for (auto& element : result.elements) {
            element.x = std::clamp(element.x, result.anchorX,
                                   result.anchorX + result.anchorWidth);
            element.y = std::clamp(element.y, result.anchorY,
                                   result.anchorY + result.anchorHeight);
            element.width = std::clamp(element.width, 0.0f,
                                       result.anchorX + result.anchorWidth - element.x);
            element.height = std::clamp(element.height, 0.0f,
                                        result.anchorY + result.anchorHeight - element.y);
            if (element.outTime >= 0.0f && element.outTime < element.inTime)
                element.outTime = element.inTime;
        }
        return result;
    }

private:
    static QString anchorName(SurfaceFXAnchorType type) {
        switch (type) {
        case SurfaceFXAnchorType::Planar: return QStringLiteral("planar");
        case SurfaceFXAnchorType::TrackedPlanar: return QStringLiteral("trackedPlanar");
        case SurfaceFXAnchorType::WorldSurface: return QStringLiteral("worldSurface");
        case SurfaceFXAnchorType::ScreenSpace: default: return QStringLiteral("screenSpace");
        }
    }

    static SurfaceFXAnchorType anchorFromName(const QString& name) {
        if (name == QStringLiteral("planar")) return SurfaceFXAnchorType::Planar;
        if (name == QStringLiteral("trackedPlanar")) return SurfaceFXAnchorType::TrackedPlanar;
        if (name == QStringLiteral("worldSurface")) return SurfaceFXAnchorType::WorldSurface;
        return SurfaceFXAnchorType::ScreenSpace;
    }
};

inline const bool kSurfaceFXSerializationRegistered = [] {
    Serialization::registerJsonSerializableType<SurfaceFXElement>(
        QStringLiteral("SurfaceFXElement"), 3);
    Serialization::registerJsonSerializableType<SurfaceFXData>(
        QStringLiteral("SurfaceFXData"), 3);
    auto& migrations = Serialization::SchemaMigrationRegistry::instance();
    migrations.registerMigration(QStringLiteral("SurfaceFXElement"), 0, 1,
                                  [](const QJsonObject& object) { return object; });
    migrations.registerMigration(QStringLiteral("SurfaceFXData"), 0, 1,
                                  [](const QJsonObject& object) { return object; });
    migrations.registerMigration(QStringLiteral("SurfaceFXElement"), 1, 2,
                                  [](const QJsonObject& object) { return object; });
    migrations.registerMigration(QStringLiteral("SurfaceFXData"), 1, 2,
                                  [](const QJsonObject& object) { return object; });
    migrations.registerMigration(QStringLiteral("SurfaceFXElement"), 2, 3,
                                  [](const QJsonObject& object) { return object; });
    migrations.registerMigration(QStringLiteral("SurfaceFXData"), 2, 3,
                                  [](const QJsonObject& object) { return object; });
    return true;
}();

} // namespace ArtifactCore
