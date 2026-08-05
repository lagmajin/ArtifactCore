module;
#include <algorithm>
#include <QJsonObject>
#include <QSize>
#include <QString>

export module Asset.Import.Setting;

export namespace ArtifactCore {

struct AssetImportSettings {
    QSize proxyResolution = QSize(1920, 1080);
    int jpegQuality = 85;
    bool generateProxyOnImport = false;
    bool normalizeColorSpace = true;
    QString workingColorSpace = QStringLiteral("Linear");

    QJsonObject toJson() const {
        return {
            {QStringLiteral("proxyWidth"), proxyResolution.width()},
            {QStringLiteral("proxyHeight"), proxyResolution.height()},
            {QStringLiteral("jpegQuality"), jpegQuality},
            {QStringLiteral("generateProxyOnImport"), generateProxyOnImport},
            {QStringLiteral("normalizeColorSpace"), normalizeColorSpace},
            {QStringLiteral("workingColorSpace"), workingColorSpace}
        };
    }

    static AssetImportSettings fromJson(const QJsonObject& data) {
        AssetImportSettings result;
        result.proxyResolution = QSize(
            std::max(1, data.value(QStringLiteral("proxyWidth")).toInt(result.proxyResolution.width())),
            std::max(1, data.value(QStringLiteral("proxyHeight")).toInt(result.proxyResolution.height())));
        result.jpegQuality = std::clamp(
            data.value(QStringLiteral("jpegQuality")).toInt(result.jpegQuality), 1, 100);
        result.generateProxyOnImport = data.value(QStringLiteral("generateProxyOnImport"))
            .toBool(result.generateProxyOnImport);
        result.normalizeColorSpace = data.value(QStringLiteral("normalizeColorSpace"))
            .toBool(result.normalizeColorSpace);
        result.workingColorSpace = data.value(QStringLiteral("workingColorSpace"))
            .toString(result.workingColorSpace);
        return result;
    }
};

}
