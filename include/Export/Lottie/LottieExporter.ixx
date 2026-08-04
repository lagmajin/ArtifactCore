module;
#include <QString>
#include <QJsonObject>
#include <optional>
#include <vector>

export module Export.Lottie.Exporter;

import Export.Lottie.Types;

export namespace ArtifactCore::Export::Lottie {

struct LottieExportOptions {
    bool prettyPrint = false;
    bool embedImages = true;
    bool compressKeyframes = true;
    bool strictMode = false;
    double keyframeTolerance = 0.01;
    QString name;
};

class LottieExporter {
public:
    static std::optional<LottieImageAsset> makeEmbeddedImageAsset(
        const QString& imagePath, const QString& assetId);
    static void compressKeyframes(std::vector<LottieKeyframe>& keyframes,
                                  double tolerance = 0.01);
    static bool validate(const LottieDocument& document,
                         QString* errorMessage = nullptr);
    static QString toJson(const LottieDocument& document,
                          bool prettyPrint = false);
    static bool exportToFile(const LottieDocument& document,
                             const QString& outputPath,
                             const LottieExportOptions& options = {});
    static std::optional<LottieDocument> importFromFile(
        const QString& inputPath, QString* errorMessage = nullptr);
};

} // namespace ArtifactCore::Export::Lottie
