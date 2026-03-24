module;
#include <QFile>
#include <QFileInfo>
#include <QDataStream>
#include <QByteArray>
#include <QStringList>
#include <QStringConverter>
#include <QDebug>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <array>
#include <cmath>

module Image.PSDDocument;

namespace ArtifactCore {

namespace {

constexpr char kPsdSignature[] = {'8', 'B', 'P', 'S'};
constexpr char kPsdLayerSignature[] = {'8', 'B', 'I', 'M'};

bool readExact(QDataStream& stream, char* dst, const int size)
{
    return stream.readRawData(dst, size) == size;
}

QString readAsciiString(const QByteArray& bytes)
{
    return QString::fromLatin1(bytes);
}

PsdLayerInfo parseLayerRecord(QDataStream& stream, QVector<QString>* warnings)
{
    PsdLayerInfo layer;

    qint32 top = 0;
    qint32 left = 0;
    qint32 bottom = 0;
    qint32 right = 0;
    stream >> top >> left >> bottom >> right;
    layer.bounds = QRect(left, top, std::max(0, right - left), std::max(0, bottom - top));

    quint16 channelCount = 0;
    stream >> channelCount;
    layer.channels.reserve(channelCount);
    for (quint16 i = 0; i < channelCount; ++i) {
        PsdChannelInfo channel;
        stream >> channel.channelId;
        stream >> channel.dataLength;
        layer.channels.push_back(channel);
    }

    char layerSig[4] = {};
    char blendKey[4] = {};
    if (!readExact(stream, layerSig, 4) || !readExact(stream, blendKey, 4)) {
        if (warnings) {
            warnings->push_back(QStringLiteral("PSD layer record ended before blend mode key."));
        }
        return layer;
    }
    if (std::memcmp(layerSig, kPsdLayerSignature, 4) != 0 && warnings) {
        warnings->push_back(QStringLiteral("PSD layer signature was not 8BIM."));
    }
    layer.blendModeKey = QByteArray(blendKey, 4);
    layer.blendMode = toBlendMode(layer.blendModeKey);

    quint8 opacity = 255;
    quint8 clipping = 0;
    quint8 flags = 0;
    quint8 filler = 0;
    stream >> opacity >> clipping >> flags >> filler;
    layer.opacity = opacity;
    layer.clipped = clipping != 0;
    layer.transparencyProtected = (flags & 0x01) != 0;
    layer.visible = (flags & 0x02) == 0;

    quint32 extraDataLength = 0;
    stream >> extraDataLength;
    const qint64 extraDataStart = stream.device() ? stream.device()->pos() : -1;
    const qint64 extraDataEnd = extraDataStart >= 0 ? extraDataStart + static_cast<qint64>(extraDataLength) : -1;

    if (extraDataLength >= 8) {
        quint32 maskLength = 0;
        quint32 rangesLength = 0;
        stream >> maskLength;
        if (maskLength > 0 && stream.device()) {
            stream.device()->seek(stream.device()->pos() + static_cast<qint64>(maskLength));
        }
        stream >> rangesLength;
        if (rangesLength > 0 && stream.device()) {
            stream.device()->seek(stream.device()->pos() + static_cast<qint64>(rangesLength));
        }

        if (stream.device() && stream.device()->pos() < extraDataEnd) {
            quint8 nameLength = 0;
            stream >> nameLength;
            QByteArray nameBytes;
            nameBytes.resize(nameLength);
            if (nameLength > 0) {
                stream.readRawData(nameBytes.data(), nameLength);
            }
            layer.name = readAsciiString(nameBytes).trimmed();
        }
    }

    if (stream.device() && extraDataEnd >= 0) {
        stream.device()->seek(extraDataEnd);
    }

    if (layer.name.isEmpty()) {
        layer.name = QStringLiteral("Layer");
    }

    return layer;
}

QString blendModeLabelFromKey(const QByteArray& key)
{
    const QByteArray normalized = key.left(4);
    if (normalized == "norm") return QStringLiteral("Normal");
    if (normalized == "diss") return QStringLiteral("Normal");
    if (normalized == "add ") return QStringLiteral("Add");
    if (normalized == "sub ") return QStringLiteral("Subtract");
    if (normalized == "mul ") return QStringLiteral("Multiply");
    if (normalized == "scrn") return QStringLiteral("Screen");
    if (normalized == "over") return QStringLiteral("Overlay");
    if (normalized == "dark") return QStringLiteral("Darken");
    if (normalized == "lite") return QStringLiteral("Lighten");
    if (normalized == "div ") return QStringLiteral("Difference");
    if (normalized == "diff") return QStringLiteral("Difference");
    if (normalized == "excl") return QStringLiteral("Exclusion");
    if (normalized == "hLit") return QStringLiteral("Hard Light");
    if (normalized == "sLit") return QStringLiteral("Soft Light");
    if (normalized == "hue ") return QStringLiteral("Hue");
    if (normalized == "sat ") return QStringLiteral("Saturation");
    if (normalized == "colr") return QStringLiteral("Color");
    if (normalized == "lum ") return QStringLiteral("Luminosity");
    if (normalized == "idiv") return QStringLiteral("Color Dodge");
    if (normalized == "smud") return QStringLiteral("Color Burn");
    return QStringLiteral("Normal");
}

RawImage loadFlattenedPreview(const QString& filePath, const PsdHeader& header)
{
    RawImage image;
    if (filePath.isEmpty()) {
        return image;
    }

    const QByteArray utf8 = QFile::encodeName(filePath);
    int w = 0;
    int h = 0;
    int comp = 0;

    if (header.depth >= 16 && stbi_is_16_bit(utf8.constData())) {
        stbi_us* pixels16 = stbi_load_16(utf8.constData(), &w, &h, &comp, 4);
        if (!pixels16) {
            return image;
        }
        image.width = w;
        image.height = h;
        image.channels = 4;
        image.pixelType = QStringLiteral("uint16");
        image.data.resize(static_cast<int>(w * h * 4 * static_cast<int>(sizeof(stbi_us))));
        std::memcpy(image.data.data(), pixels16, image.data.size());
        stbi_image_free(pixels16);
        return image;
    }

    stbi_uc* pixels = stbi_load(utf8.constData(), &w, &h, &comp, 4);
    if (!pixels) {
        return image;
    }
    image.width = w;
    image.height = h;
    image.channels = 4;
    image.pixelType = QStringLiteral("uint8");
    image.data.resize(static_cast<int>(w * h * 4));
    std::memcpy(image.data.data(), pixels, image.data.size());
    stbi_image_free(pixels);
    return image;
}

} // namespace

class PsdDocument::Impl {
public:
    QString filePath;
    QString errorString;
    PsdHeader header;
    PsdSectionInfo sections;
    QVector<PsdLayerInfo> layers;
    QVector<QString> warnings;
    bool open = false;
    bool hasLayerMetadata = false;

    void reset()
    {
        filePath.clear();
        errorString.clear();
        header = {};
        sections = {};
        layers.clear();
        warnings.clear();
        open = false;
        hasLayerMetadata = false;
    }

    bool parse(const QString& path)
    {
        reset();
        filePath = path;
        header.filePath = path;

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            errorString = QStringLiteral("Failed to open PSD file.");
            return false;
        }

        QDataStream stream(&file);
        stream.setByteOrder(QDataStream::BigEndian);

        char signature[4] = {};
        if (!readExact(stream, signature, 4) || std::memcmp(signature, kPsdSignature, 4) != 0) {
            errorString = QStringLiteral("Not a PSD/PSB file.");
            return false;
        }

        quint16 version = 0;
        stream >> version;
        if (version != 1 && version != 2) {
            errorString = QStringLiteral("Unsupported Photoshop document version.");
            return false;
        }
        header.version = version;

        char reserved[6] = {};
        if (!readExact(stream, reserved, 6)) {
            errorString = QStringLiteral("PSD header is truncated.");
            return false;
        }

        stream >> header.channelCount;
        stream >> header.height;
        stream >> header.width;
        stream >> header.depth;
        quint16 colorMode = 0;
        stream >> colorMode;
        header.colorMode = toPsdColorMode(colorMode);

        if (!header.isValid()) {
            errorString = QStringLiteral("PSD header contained invalid dimensions.");
            return false;
        }

        stream >> sections.colorModeDataLength;
        sections.colorModeDataOffset = file.pos();
        if (sections.colorModeDataLength > 0) {
            file.seek(sections.colorModeDataOffset + static_cast<qint64>(sections.colorModeDataLength));
        }

        stream >> sections.imageResourcesLength;
        sections.imageResourcesOffset = file.pos();
        if (sections.imageResourcesLength > 0) {
            file.seek(sections.imageResourcesOffset + static_cast<qint64>(sections.imageResourcesLength));
        }

        stream >> sections.layerAndMaskInfoLength;
        sections.layerAndMaskInfoOffset = file.pos();
        if (sections.layerAndMaskInfoLength > 0) {
            const qint64 layerInfoBlockStart = file.pos();
            const qint64 layerInfoBlockEnd = layerInfoBlockStart + static_cast<qint64>(sections.layerAndMaskInfoLength);
            quint32 layerInfoLength = 0;
            stream >> layerInfoLength;
            if (layerInfoLength > 0) {
                const qint64 layerInfoStart = file.pos();
                qint16 layerCountSigned = 0;
                stream >> layerCountSigned;
                const int layerCount = std::abs(layerCountSigned);
                layers.reserve(layerCount);
                for (int i = 0; i < layerCount; ++i) {
                    layers.push_back(parseLayerRecord(stream, &warnings));
                }
                hasLayerMetadata = !layers.isEmpty();
            }
            file.seek(layerInfoBlockEnd);
        }

        sections.imageDataOffset = file.pos();
        sections.imageDataLength = static_cast<quint32>(std::max<qint64>(0, file.size() - file.pos()));
        open = true;
        return true;
    }
};

PsdDocument::PsdDocument()
    : impl_(new Impl())
{
}

PsdDocument::~PsdDocument()
{
    delete impl_;
}

PsdDocument::PsdDocument(PsdDocument&& other) noexcept
    : impl_(other.impl_)
{
    other.impl_ = new Impl();
}

PsdDocument& PsdDocument::operator=(PsdDocument&& other) noexcept
{
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = new Impl();
    }
    return *this;
}

bool PsdDocument::open(const QString& filePath)
{
    return impl_ && impl_->parse(filePath);
}

void PsdDocument::close()
{
    if (impl_) {
        impl_->reset();
    }
}

bool PsdDocument::isOpen() const
{
    return impl_ && impl_->open;
}

QString PsdDocument::filePath() const
{
    return impl_ ? impl_->filePath : QString();
}

QString PsdDocument::errorString() const
{
    return impl_ ? impl_->errorString : QString();
}

const PsdHeader& PsdDocument::header() const
{
    static const PsdHeader empty;
    return impl_ ? impl_->header : empty;
}

const PsdSectionInfo& PsdDocument::sectionInfo() const
{
    static const PsdSectionInfo empty;
    return impl_ ? impl_->sections : empty;
}

const QVector<PsdLayerInfo>& PsdDocument::layers() const
{
    static const QVector<PsdLayerInfo> empty;
    return impl_ ? impl_->layers : empty;
}

const QVector<QString>& PsdDocument::warnings() const
{
    static const QVector<QString> empty;
    return impl_ ? impl_->warnings : empty;
}

bool PsdDocument::hasLayerMetadata() const
{
    return impl_ && impl_->hasLayerMetadata;
}

RawImage PsdDocument::flattenedPreview() const
{
    if (!impl_ || !impl_->open) {
        return {};
    }
    return loadFlattenedPreview(impl_->filePath, impl_->header);
}

PsdCompression toPsdCompression(const quint16 value)
{
    switch (value) {
    case 0: return PsdCompression::Raw;
    case 1: return PsdCompression::Rle;
    case 2: return PsdCompression::Zip;
    case 3: return PsdCompression::ZipPrediction;
    default: return PsdCompression::Unknown;
    }
}

PsdColorMode toPsdColorMode(const quint16 value)
{
    switch (value) {
    case 0: return PsdColorMode::Bitmap;
    case 1: return PsdColorMode::Grayscale;
    case 2: return PsdColorMode::Indexed;
    case 3: return PsdColorMode::RGB;
    case 4: return PsdColorMode::CMYK;
    case 7: return PsdColorMode::Multichannel;
    case 8: return PsdColorMode::Duotone;
    case 9: return PsdColorMode::Lab;
    default: return PsdColorMode::Unknown;
    }
}

ArtifactCore::BlendMode toBlendMode(const QByteArray& key)
{
    const QByteArray normalized = key.left(4);
    if (normalized == "add ") return ArtifactCore::BlendMode::Add;
    if (normalized == "sub ") return ArtifactCore::BlendMode::Subtract;
    if (normalized == "mul ") return ArtifactCore::BlendMode::Multiply;
    if (normalized == "scrn") return ArtifactCore::BlendMode::Screen;
    if (normalized == "over") return ArtifactCore::BlendMode::Overlay;
    if (normalized == "dark") return ArtifactCore::BlendMode::Darken;
    if (normalized == "lite") return ArtifactCore::BlendMode::Lighten;
    if (normalized == "idiv") return ArtifactCore::BlendMode::ColorDodge;
    if (normalized == "smud") return ArtifactCore::BlendMode::ColorBurn;
    if (normalized == "hLit") return ArtifactCore::BlendMode::HardLight;
    if (normalized == "sLit") return ArtifactCore::BlendMode::SoftLight;
    if (normalized == "diff") return ArtifactCore::BlendMode::Difference;
    if (normalized == "excl") return ArtifactCore::BlendMode::Exclusion;
    if (normalized == "hue ") return ArtifactCore::BlendMode::Hue;
    if (normalized == "sat ") return ArtifactCore::BlendMode::Saturation;
    if (normalized == "colr") return ArtifactCore::BlendMode::Color;
    if (normalized == "lum ") return ArtifactCore::BlendMode::Luminosity;
    return ArtifactCore::BlendMode::Normal;
}

QString toBlendModeLabel(const QByteArray& key)
{
    return blendModeLabelFromKey(key);
}

bool PsdHeader::isValid() const
{
    return width > 0 && height > 0 && channelCount > 0;
}

bool PsdHeader::isLargeDocument() const
{
    return version == 2;
}

} // namespace ArtifactCore
