module;
#include <QString>
#include <QRect>
#include <QVector>
#include <QByteArray>

#include <cstdint>

export module Image.PSDDocument;

import Image.Raw;
import Layer.Blend;

export namespace ArtifactCore {

enum class PsdColorMode {
    Bitmap = 0,
    Grayscale = 1,
    Indexed = 2,
    RGB = 3,
    CMYK = 4,
    Multichannel = 7,
    Duotone = 8,
    Lab = 9,
    Unknown = -1
};

enum class PsdCompression {
    Raw = 0,
    Rle = 1,
    Zip = 2,
    ZipPrediction = 3,
    Unknown = -1
};

struct PsdHeader {
    QString filePath;
    quint16 version = 1;
    quint16 channelCount = 0;
    quint32 width = 0;
    quint32 height = 0;
    quint16 depth = 8;
    PsdColorMode colorMode = PsdColorMode::Unknown;

    bool isValid() const;
    bool isLargeDocument() const;
};

struct PsdSectionInfo {
    qint64 colorModeDataOffset = 0;
    quint32 colorModeDataLength = 0;
    qint64 imageResourcesOffset = 0;
    quint32 imageResourcesLength = 0;
    qint64 layerAndMaskInfoOffset = 0;
    quint32 layerAndMaskInfoLength = 0;
    qint64 imageDataOffset = 0;
    quint32 imageDataLength = 0;
};

struct PsdChannelInfo {
    qint16 channelId = 0;
    quint32 dataLength = 0;
};

struct PsdLayerInfo {
    QRect bounds;
    QVector<PsdChannelInfo> channels;
    QString name;
    ArtifactCore::BlendMode blendMode = ArtifactCore::BlendMode::Normal;
    QByteArray blendModeKey;
    quint8 opacity = 255;
    bool visible = true;
    bool transparencyProtected = false;
    bool clipped = false;
};

class PsdDocument {
public:
    PsdDocument();
    ~PsdDocument();

    PsdDocument(const PsdDocument&) = delete;
    PsdDocument& operator=(const PsdDocument&) = delete;
    PsdDocument(PsdDocument&&) noexcept;
    PsdDocument& operator=(PsdDocument&&) noexcept;

    bool open(const QString& filePath);
    void close();

    bool isOpen() const;
    QString filePath() const;
    QString errorString() const;

    const PsdHeader& header() const;
    const PsdSectionInfo& sectionInfo() const;
    const QVector<PsdLayerInfo>& layers() const;
    const QVector<QString>& warnings() const;

    bool hasLayerMetadata() const;
    RawImage flattenedPreview() const;

private:
    class Impl;
    Impl* impl_;
};

PsdCompression toPsdCompression(quint16 value);
PsdColorMode toPsdColorMode(quint16 value);
ArtifactCore::BlendMode toBlendMode(const QByteArray& key);
QString toBlendModeLabel(const QByteArray& key);

} // namespace ArtifactCore
