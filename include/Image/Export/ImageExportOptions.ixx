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
    // Optional integer bit depth for formats that store fewer bits than the
    // selected storage type (for example 10/12-bit DPX in UINT16 samples).
    int bitsPerSample = 0;
    QString colorSpace = "sRGB";
    // Optional explicit pixel transfer conversion. When outputTransferFunction
    // is set, RGB is decoded from inputTransferFunction to scene-linear and
    // encoded with outputTransferFunction before quantization. Empty values
    // preserve the existing pixel path; callers must state both ends.
    QString inputTransferFunction;
    QString outputTransferFunction;
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
