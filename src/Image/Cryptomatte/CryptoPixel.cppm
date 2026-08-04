module;
#include <algorithm>
#include <cmath>
#include <cstring>
#include <QString>

module Image.Cryptomatte.Pixel;

namespace ArtifactCore::Image::Cryptomatte {
namespace {
std::uint32_t hashName(const QByteArray& bytes) {
    // MurmurHash3 x86 32, truncated to Cryptomatte's 24-bit payload.
    const auto* data = reinterpret_cast<const std::uint8_t*>(bytes.constData());
    const std::size_t length = static_cast<std::size_t>(bytes.size());
    std::uint32_t hash = 0;
    constexpr std::uint32_t c1 = 0xcc9e2d51u;
    constexpr std::uint32_t c2 = 0x1b873593u;
    const std::size_t blocks = length / 4;
    for (std::size_t i = 0; i < blocks; ++i) {
        std::uint32_t k = 0;
        std::memcpy(&k, data + i * 4, sizeof(k));
        k *= c1;
        k = (k << 15) | (k >> 17);
        k *= c2;
        hash ^= k;
        hash = (hash << 13) | (hash >> 19);
        hash = hash * 5u + 0xe6546b64u;
    }
    const auto* tail = data + blocks * 4;
    std::uint32_t k = 0;
    switch (length & 3u) {
    case 3: k ^= static_cast<std::uint32_t>(tail[2]) << 16; [[fallthrough]];
    case 2: k ^= static_cast<std::uint32_t>(tail[1]) << 8; [[fallthrough]];
    case 1:
        k ^= tail[0];
        k *= c1;
        k = (k << 15) | (k >> 17);
        k *= c2;
        hash ^= k;
        break;
    default: break;
    }
    hash ^= static_cast<std::uint32_t>(length);
    hash ^= hash >> 16;
    hash *= 0x85ebca6bu;
    hash ^= hash >> 13;
    hash *= 0xc2b2ae35u;
    hash ^= hash >> 16;
    return hash & 0x00ffffffu;
}
}

std::uint32_t CryptoSample::nameToId(const QString& objectName) {
    return hashName(objectName.toUtf8());
}

void CryptoPixel::addSample(float id, float coverage) {
    if (!std::isfinite(id) || !std::isfinite(coverage) || coverage <= 0.0f) return;
    coverage = std::clamp(coverage, 0.0f, 1.0f);
    for (int i = 0; i < count_; ++i) {
        if (samples_[i].id == id) {
            samples_[i].coverage = std::clamp(samples_[i].coverage + coverage, 0.0f, 1.0f);
            return;
        }
    }
    if (count_ < kMaxSamples) {
        samples_[count_++] = CryptoSample{id, coverage};
    } else {
        auto weakest = std::min_element(samples_, samples_ + count_,
                                         [](const auto& a, const auto& b) {
                                             return a.coverage < b.coverage;
                                         });
        if (weakest != samples_ + count_ && coverage > weakest->coverage)
            *weakest = CryptoSample{id, coverage};
    }
    sort();
}

void CryptoPixel::addSample(const QString& objectName, float coverage) {
    addSample(static_cast<float>(CryptoSample::nameToId(objectName)), coverage);
}

void CryptoPixel::clear() { count_ = 0; }

void CryptoPixel::sort() {
    std::stable_sort(samples_, samples_ + count_, [](const auto& a, const auto& b) {
        return a.coverage > b.coverage;
    });
}

void CryptoPixel::getSamplesForRank(int rank, std::vector<CryptoSample>& out) const {
    out.clear();
    if (rank < 0) return;
    const int first = rank * 2;
    for (int i = first; i < std::min(first + 2, count_); ++i) out.push_back(samples_[i]);
}

} // namespace ArtifactCore::Image::Cryptomatte
