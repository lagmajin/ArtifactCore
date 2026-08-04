module;
#include <cstdint>
#include <memory>
#include <QString>
#include <vector>

export module Image.Cryptomatte.Image;

import Image.Cryptomatte.Pixel;

namespace ArtifactCore::Image::Cryptomatte {

export enum class CryptoLayer { Object, Material, Asset };

export struct CryptoManifestEntry {
    QString originalName;
    std::uint32_t hash = 0;
};

export class CryptoManifest {
public:
    void addEntry(const QString& name);
    QString findName(std::uint32_t hash) const;
    std::uint32_t findHash(const QString& name) const;
    int entryCount() const { return static_cast<int>(entries_.size()); }
    const std::vector<CryptoManifestEntry>& entries() const { return entries_; }
    QString toMetadata() const;

private:
    std::vector<CryptoManifestEntry> entries_;
};

export class CryptoImage {
public:
    CryptoImage() = default;
    CryptoImage(int width, int height);
    bool resize(int width, int height);
    void fromObjectIdBuffer(const std::vector<float>& ids, int width, int height);
    void fromMaterialIdBuffer(const std::vector<float>& ids, int width, int height);
    void fromIdCoverageBuffers(const std::vector<float>& ids,
                               const std::vector<float>& coverage,
                               int width, int height);
    void generateRankLayers(CryptoLayer layer, int rank,
                            std::vector<float>& outR, std::vector<float>& outG) const;
    void generateRankLayers(CryptoLayer layer, int rank,
                            std::vector<float>& outId0, std::vector<float>& outCoverage0,
                            std::vector<float>& outId1, std::vector<float>& outCoverage1) const;
    int maxRank() const;
    int width() const { return width_; }
    int height() const { return height_; }
    CryptoManifest& manifest() { return manifest_; }
    const CryptoManifest& manifest() const { return manifest_; }

private:
    void fromIdBuffer(const std::vector<float>& ids, int width, int height);
    int width_ = 0;
    int height_ = 0;
    std::unique_ptr<CryptoPixel[]> pixels_;
    CryptoManifest manifest_;
};

} // namespace ArtifactCore::Image::Cryptomatte
