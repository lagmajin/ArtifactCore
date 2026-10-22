module;
#include <algorithm>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/typedesc.h>
#include <QFileInfo>
#include <QStringList>
#include <vector>

module Image.Cryptomatte.ExrWriter;

import Image.Cryptomatte.Pixel;

namespace ArtifactCore::Image::Cryptomatte {
namespace {
QString layerName(CryptoLayer layer) {
    switch (layer) {
    case CryptoLayer::Material: return QStringLiteral("CryptoMaterial");
    case CryptoLayer::Asset: return QStringLiteral("CryptoAsset");
    default: return QStringLiteral("CryptoObject");
    }
}
}

CryptoExrWriteResult CryptoExrWriter::write(const QString& filePath,
                                           const CryptoImage& image,
                                           CryptoLayer layer,
                                           int ranks) {
    CryptoExrWriteResult result;
    if (filePath.trimmed().isEmpty() || image.width() <= 0 || image.height() <= 0)
        { result.error = QStringLiteral("Invalid Cryptomatte output or image."); return result; }
    const int availableRanks = image.maxRank();
    if (ranks <= 0) ranks = std::max(1, availableRanks);
    if (ranks > 8) ranks = 8;

    const int channels = ranks * 4;
    OIIO::ImageSpec spec(image.width(), image.height(), channels, OIIO::TypeDesc::FLOAT);
    const QString prefix = layerName(layer);
    const char* suffixes[] = {"R", "G", "B", "A"};
    spec.channelnames.clear();
    for (int rank = 0; rank < ranks; ++rank) {
        for (const char* suffix : suffixes)
            spec.channelnames.push_back(QStringLiteral("%1%2.%3")
                                            .arg(prefix)
                                            .arg(rank, 2, 10, QLatin1Char('0'))
                                            .arg(QLatin1String(suffix)).toUtf8().constData());
    }
    const QString manifestId = QString::number(CryptoSample::nameToId(prefix), 16)
                                   .rightJustified(8, QLatin1Char('0')).toUpper();
    const QString metadataPrefix = QStringLiteral("cryptomatte/%1/").arg(manifestId);
    const QByteArray manifest = image.manifest().toMetadata().toUtf8();
    const QByteArray metadataName = (metadataPrefix + QStringLiteral("name")).toUtf8();
    const QByteArray metadataHash = (metadataPrefix + QStringLiteral("hash")).toUtf8();
    const QByteArray metadataManifest = (metadataPrefix + QStringLiteral("manifest")).toUtf8();
    spec.attribute(metadataName.constData(), prefix.toUtf8().constData());
    spec.attribute(metadataHash.constData(), "MurmurHash3_32");
    spec.attribute(metadataManifest.constData(), manifest.constData());
    spec.attribute("cryptomatte/num_layers", ranks);

    std::vector<float> interleaved(static_cast<std::size_t>(image.width()) * image.height() * channels, 0.0f);
    std::vector<float> id0, coverage0, id1, coverage1;
    for (int rank = 0; rank < ranks; ++rank) {
        image.generateRankLayers(layer, rank, id0, coverage0, id1, coverage1);
        for (std::size_t pixel = 0; pixel < id0.size(); ++pixel) {
            const std::size_t offset = pixel * static_cast<std::size_t>(channels) + rank * 4;
            interleaved[offset + 0] = id0[pixel];
            interleaved[offset + 1] = coverage0[pixel];
            interleaved[offset + 2] = id1[pixel];
            interleaved[offset + 3] = coverage1[pixel];
        }
    }

    const QByteArray path = QFileInfo(filePath).absoluteFilePath().toUtf8();
    auto output = OIIO::ImageOutput::create(path.constData());
    if (!output) { result.error = QStringLiteral("Unable to create EXR writer."); return result; }
    if (!output->open(path.constData(), spec) ||
        !output->write_image(OIIO::TypeDesc::FLOAT, interleaved.data())) {
        result.error = QString::fromUtf8(output->geterror().c_str());
        output->close();
        return result;
    }
    output->close();
    result.success = true;
    return result;
}

} // namespace ArtifactCore::Image::Cryptomatte
