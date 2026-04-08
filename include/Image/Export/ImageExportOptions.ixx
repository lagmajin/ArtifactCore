module;
#include <utility>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/typedesc.h>
#include <QString>

export module Image.ExportOptions;

export namespace ArtifactCore {

struct ImageExportOptions {
    QString format;
    QString compression;
    float compressionQuality = 90.0f;
    OIIO::TypeDesc dataType = OIIO::TypeDesc::UINT8;
    QString colorSpace = "sRGB";
    int tileWidth = 0;
    int tileHeight = 0;
    QString creator;
    QString copyright;
};

}
