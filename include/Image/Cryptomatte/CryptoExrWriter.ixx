module;
#include <QString>

export module Image.Cryptomatte.ExrWriter;

import Image.Cryptomatte.Image;

namespace ArtifactCore::Image::Cryptomatte {

export struct CryptoExrWriteResult {
    bool success = false;
    QString error;
    operator bool() const { return success; }
};

export class CryptoExrWriter {
public:
    static CryptoExrWriteResult write(const QString& filePath,
                                      const CryptoImage& image,
                                      CryptoLayer layer = CryptoLayer::Object,
                                      int ranks = 0);
};

} // namespace ArtifactCore::Image::Cryptomatte
