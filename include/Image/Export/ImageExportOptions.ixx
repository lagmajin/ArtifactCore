module;
#include <utility>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/typedesc.h>
#include <QMap>
#include <QByteArray>
#include <QString>

export module Image.ExportOptions;

export namespace ArtifactCore {

struct ImageExportOptions {
    QString format;
    QString compression;
    float compressionQuality = 90.0f;
    OIIO::TypeDesc dataType = OIIO::TypeDesc::UINT8;
    QString colorSpace = "sRGB";
    // Optional ICC profile payload. When set, ImageExporter embeds it in
    // formats that support ICC metadata (PNG/JPEG/TIFF/EXR as supported by
    // the active OIIO plugin).
    QByteArray iccProfileData;
    QString iccProfilePath;
    int tileWidth = 0;
    int tileHeight = 0;
    QString creator;
    QString copyright;
    QMap<QString, QString> stringAttributes;
};

}
