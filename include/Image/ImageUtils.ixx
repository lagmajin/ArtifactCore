module;
#include <utility>
#include <OpenImageIO/imageio.h>
#include <QImage>

export module Image.Utils;

import Image.ExportOptions;

export namespace ArtifactCore {

/**
 * @brief Create an OIIO ImageSpec from export options.
 */
inline OIIO::ImageSpec makeSpec(int width, int height, const ImageExportOptions& opt) {
    using namespace OIIO;
    int nchannels = 4; // Standard RGBA output
    ImageSpec spec(width, height, nchannels, opt.dataType);
    
    if (!opt.compression.isEmpty()) {
        spec.attribute("compression", opt.compression.toUtf8().constData());
    }
    
    if (opt.compressionQuality >= 0) {
        spec.attribute("CompressionQuality", opt.compressionQuality);
    }

    if (opt.tileWidth > 0 && opt.tileHeight > 0) {
        spec.tile_width = opt.tileWidth;
        spec.tile_height = opt.tileHeight;
    }

    spec.set_colorspace(opt.colorSpace.toUtf8().constData());
    
    if (!opt.creator.isEmpty()) {
        spec.attribute("Artist", opt.creator.toUtf8().constData());
    }
    
    if (!opt.copyright.isEmpty()) {
        spec.attribute("Copyright", opt.copyright.toUtf8().constData());
    }

    return spec;
}

}
