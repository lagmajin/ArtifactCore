module;
#include <cstdint>
#include <QString>
#include <vector>

export module Image.Cryptomatte.Pixel;

namespace ArtifactCore::Image::Cryptomatte {

export struct CryptoSample {
    float id = 0.0f;
    float coverage = 0.0f;
    static std::uint32_t nameToId(const QString& objectName);
};

export class CryptoPixel {
public:
    static constexpr int kMaxSamples = 16;

    void addSample(float id, float coverage);
    void addSample(const QString& objectName, float coverage);
    void clear();
    int sampleCount() const { return count_; }
    bool isEmpty() const { return count_ == 0; }
    void sort();
    void getSamplesForRank(int rank, std::vector<CryptoSample>& out) const;

private:
    CryptoSample samples_[kMaxSamples]{};
    int count_ = 0;
};

} // namespace ArtifactCore::Image::Cryptomatte
