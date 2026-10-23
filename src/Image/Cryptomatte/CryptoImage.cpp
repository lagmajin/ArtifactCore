module;
#include <algorithm>
#include <cmath>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <utility>

module Image.Cryptomatte.Image;

namespace ArtifactCore::Image::Cryptomatte {

void CryptoManifest::addEntry(const QString& name) {
    if (name.isEmpty()) return;
    const auto hash = CryptoSample::nameToId(name);
    for (const auto& entry : entries_)
        if (entry.originalName == name) return;
    // A manifest cannot represent two names with the same encoded ID.
    // Keep the first entry rather than silently emitting ambiguous metadata.
    for (const auto& entry : entries_)
        if (entry.hash == hash) return;
    entries_.push_back({name, hash});
}

QString CryptoManifest::findName(std::uint32_t hash) const {
    for (const auto& entry : entries_)
        if (entry.hash == hash) return entry.originalName;
    return {};
}

std::uint32_t CryptoManifest::findHash(const QString& name) const {
    if (name.isEmpty()) return 0;
    const auto hash = CryptoSample::nameToId(name);
    for (const auto& entry : entries_)
        if (entry.originalName == name) return entry.hash;
    return hash;
}

QString CryptoManifest::toMetadata() const {
    QJsonObject manifest;
    for (const auto& entry : entries_)
        manifest[QString::number(entry.hash, 16).rightJustified(8, QLatin1Char('0'))] = entry.originalName;
    return QString::fromUtf8(QJsonDocument(manifest).toJson(QJsonDocument::Compact));
}

CryptoImage::CryptoImage(int width, int height) { resize(width, height); }

bool CryptoImage::resize(int width, int height) {
    if (width <= 0 || height <= 0) {
        width_ = height_ = 0;
        pixels_.reset();
        return false;
    }
    const auto count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (count > static_cast<std::size_t>(-1) / sizeof(CryptoPixel)) return false;
    auto pixels = std::make_unique<CryptoPixel[]>(count);
    width_ = width;
    height_ = height;
    pixels_ = std::move(pixels);
    return true;
}

void CryptoImage::fromObjectIdBuffer(const std::vector<float>& ids, int width, int height) {
    fromIdBuffer(ids, width, height);
}

void CryptoImage::fromMaterialIdBuffer(const std::vector<float>& ids, int width, int height) {
    fromIdBuffer(ids, width, height);
}

void CryptoImage::fromIdCoverageBuffers(const std::vector<float>& ids,
                                        const std::vector<float>& coverage,
                                        int width, int height) {
    const auto count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (!resize(width, height) || ids.size() < count || coverage.size() < count) return;
    for (std::size_t i = 0; i < count; ++i) {
        if (std::isfinite(ids[i]) && std::isfinite(coverage[i]) && ids[i] > 0.0f)
            pixels_[i].addSample(ids[i], coverage[i]);
    }
}

void CryptoImage::fromIdBuffer(const std::vector<float>& ids, int width, int height) {
    if (!resize(width, height) || ids.size() < static_cast<std::size_t>(width) * height) return;
    for (std::size_t i = 0; i < static_cast<std::size_t>(width) * height; ++i) {
        if (std::isfinite(ids[i]) && ids[i] > 0.0f) pixels_[i].addSample(ids[i], 1.0f);
    }
}

int CryptoImage::maxRank() const {
    int maxSamples = 0;
    for (std::size_t i = 0; i < static_cast<std::size_t>(width_) * height_; ++i)
        maxSamples = std::max(maxSamples, pixels_[i].sampleCount());
    return (maxSamples + 1) / 2;
}

void CryptoImage::generateRankLayers(CryptoLayer, int rank,
                                     std::vector<float>& outR,
                                     std::vector<float>& outG) const {
    const auto count = static_cast<std::size_t>(width_) * height_;
    outR.assign(count, 0.0f);
    outG.assign(count, 0.0f);
    if (rank < 0 || !pixels_) return;
    std::vector<CryptoSample> samples;
    for (std::size_t i = 0; i < count; ++i) {
        pixels_[i].getSamplesForRank(rank, samples);
        if (!samples.empty()) { outR[i] = samples[0].id; outG[i] = samples[0].coverage; }
        // The second pair is emitted by the next API channel in the caller's
        // packed representation; preserve the primary pair here for R/G EXR planes.
    }
}

void CryptoImage::generateRankLayers(CryptoLayer layer, int rank,
                                     std::vector<float>& outId0,
                                     std::vector<float>& outCoverage0,
                                     std::vector<float>& outId1,
                                     std::vector<float>& outCoverage1) const {
    generateRankLayers(layer, rank, outId0, outCoverage0);
    const auto count = static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_);
    outId1.assign(count, 0.0f);
    outCoverage1.assign(count, 0.0f);
    if (rank < 0 || !pixels_) return;
    std::vector<CryptoSample> samples;
    for (std::size_t i = 0; i < count; ++i) {
        pixels_[i].getSamplesForRank(rank, samples);
        if (samples.size() > 1) {
            outId1[i] = samples[1].id;
            outCoverage1[i] = samples[1].coverage;
        }
    }
}

} // namespace ArtifactCore::Image::Cryptomatte
