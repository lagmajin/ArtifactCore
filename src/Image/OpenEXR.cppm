module;
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/typedesc.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <QFileInfo>
#include <limits>
#include <string>
#include <vector>

module Image:OpenEXR;

namespace ArtifactCore {

OpenExr::OpenExr() = default;
OpenExr::~OpenExr() = default;

bool OpenExr::writeRGBA32F(const QString& path, const float* rgba,
                           int width, int height, const QString& compression) {
    if (path.trimmed().isEmpty() || !rgba || width <= 0 || height <= 0) return false;
    const auto pixelCount = static_cast<std::size_t>(width) *
                            static_cast<std::size_t>(height);
    if (pixelCount > std::numeric_limits<std::size_t>::max() / 4u) return false;
    if (!std::all_of(rgba, rgba + pixelCount * 4u,
                     [](const float value) { return std::isfinite(value); })) return false;
    const QByteArray utf8Path = QFileInfo(path).absoluteFilePath().toUtf8();
    auto output = OIIO::ImageOutput::create(utf8Path.constData());
    if (!output) return false;
    OIIO::ImageSpec spec(width, height, 4, OIIO::TypeDesc::FLOAT);
    spec.channelnames = {"R", "G", "B", "A"};
    if (!compression.trimmed().isEmpty())
        spec.attribute("compression", compression.toUtf8().constData());
    if (!output->open(utf8Path.constData(), spec) ||
        !output->write_image(OIIO::TypeDesc::FLOAT, rgba)) {
        output->close();
        return false;
    }
    output->close();
    return true;
}

bool OpenExr::readRGBA32F(const QString& path, std::vector<float>& rgba,
                          int& width, int& height) const {
    rgba.clear();
    width = height = 0;
    if (path.trimmed().isEmpty()) return false;
    const QByteArray utf8Path = QFileInfo(path).absoluteFilePath().toUtf8();
    auto input = OIIO::ImageInput::open(utf8Path.constData());
    if (!input) return false;
    const auto& spec = input->spec();
    if (spec.width <= 0 || spec.height <= 0 || spec.nchannels < 3 ||
        spec.nchannels > 64 ||
        static_cast<std::size_t>(spec.width) >
            std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(spec.height) ||
        static_cast<std::size_t>(spec.width) * static_cast<std::size_t>(spec.height) >
            std::numeric_limits<std::size_t>::max() / 4u ||
        static_cast<std::size_t>(spec.width) * static_cast<std::size_t>(spec.height) >
            std::numeric_limits<std::size_t>::max() /
                static_cast<std::size_t>(spec.nchannels)) {
        input->close();
        return false;
    }
    width = spec.width;
    height = spec.height;
    const std::size_t pixels = static_cast<std::size_t>(width) * height;
    std::vector<float> source(pixels * static_cast<std::size_t>(spec.nchannels), 0.0f);
    if (!input->read_image(0, 0, 0, spec.nchannels,
                           OIIO::TypeDesc::FLOAT, source.data())) {
        rgba.clear();
        width = height = 0;
        input->close();
        return false;
    }
    rgba.assign(pixels * 4u, 0.0f);
    std::array<int, 4> channelIndices{-1, -1, -1, -1};
    const char* canonicalNames[] = {"r", "g", "b", "a"};
    for (int channel = 0; channel < 4; ++channel) {
        for (int candidate = 0; candidate < spec.nchannels; ++candidate) {
            if (candidate >= static_cast<int>(spec.channelnames.size())) break;
            const QString name = QString::fromStdString(spec.channelnames[candidate]).toLower();
            if (name == QLatin1String(canonicalNames[channel]) ||
                (channel == 0 && name.endsWith(QLatin1String(".r"))) ||
                (channel == 1 && name.endsWith(QLatin1String(".g"))) ||
                (channel == 2 && name.endsWith(QLatin1String(".b"))) ||
                (channel == 3 && name.endsWith(QLatin1String(".a")))) {
                channelIndices[channel] = candidate;
                break;
            }
        }
    }
    for (int channel = 0; channel < 4; ++channel) {
        if (channelIndices[channel] < 0 && channel < spec.nchannels)
            channelIndices[channel] = channel;
    }
    for (std::size_t pixel = 0; pixel < pixels; ++pixel) {
        for (int channel = 0; channel < 3; ++channel) {
            const int sourceChannel = channelIndices[channel];
            if (sourceChannel >= 0)
                rgba[pixel * 4u + static_cast<std::size_t>(channel)] =
                    source[pixel * static_cast<std::size_t>(spec.nchannels) +
                           static_cast<std::size_t>(sourceChannel)];
        }
        const int alphaChannel = channelIndices[3];
        rgba[pixel * 4u + 3u] = alphaChannel >= 0
            ? source[pixel * static_cast<std::size_t>(spec.nchannels) +
                     static_cast<std::size_t>(alphaChannel)]
            : 1.0f;
    }
    input->close();
    return true;
}

bool OpenExr::writeDeepRGBA32F(
    const QString& path, int width, int height,
    const std::vector<std::vector<OpenExrDeepSample>>& samples,
    const QString& compression) {
    if (path.trimmed().isEmpty() || width <= 0 || height <= 0) return false;
    const auto widthSize = static_cast<std::size_t>(width);
    const auto heightSize = static_cast<std::size_t>(height);
    if (widthSize > std::numeric_limits<std::size_t>::max() / heightSize)
        return false;
    const std::size_t pixelCount = widthSize * heightSize;
    if (pixelCount != samples.size()) return false;
    for (const auto& pixel : samples) {
        if (pixel.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            return false;
        if (!std::all_of(pixel.begin(), pixel.end(), [](const OpenExrDeepSample& sample) {
                return std::isfinite(sample.depth) && std::isfinite(sample.red) &&
                       std::isfinite(sample.green) && std::isfinite(sample.blue) &&
                       std::isfinite(sample.alpha);
            })) return false;
        if (!std::is_sorted(pixel.begin(), pixel.end(),
                            [](const OpenExrDeepSample& lhs,
                               const OpenExrDeepSample& rhs) {
                                return lhs.depth <= rhs.depth;
                            })) return false;
    }

    const QByteArray utf8Path = QFileInfo(path).absoluteFilePath().toUtf8();
    auto output = OIIO::ImageOutput::create(utf8Path.constData());
    if (!output) return false;
    OIIO::ImageSpec spec(width, height, 5, OIIO::TypeDesc::FLOAT);
    spec.deep = true;
    spec.channelnames = {"Z", "R", "G", "B", "A"};
    if (!compression.trimmed().isEmpty())
        spec.attribute("compression", compression.toUtf8().constData());

    OIIO::DeepData deep;
    const OIIO::TypeDesc channelTypes[] = {
        OIIO::TypeDesc::FLOAT, OIIO::TypeDesc::FLOAT, OIIO::TypeDesc::FLOAT,
        OIIO::TypeDesc::FLOAT, OIIO::TypeDesc::FLOAT};
    const std::vector<std::string> channelNames = {"Z", "R", "G", "B", "A"};
    deep.init(static_cast<int64_t>(pixelCount), 5, channelTypes, channelNames);
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        const auto& pixelSamples = samples[pixel];
        deep.set_samples(static_cast<int64_t>(pixel),
                         static_cast<int>(pixelSamples.size()));
        for (int sampleIndex = 0;
             sampleIndex < static_cast<int>(pixelSamples.size()); ++sampleIndex) {
            const auto& sample = pixelSamples[static_cast<std::size_t>(sampleIndex)];
            deep.set_deep_value(static_cast<int64_t>(pixel), 0, sampleIndex, sample.depth);
            deep.set_deep_value(static_cast<int64_t>(pixel), 1, sampleIndex, sample.red);
            deep.set_deep_value(static_cast<int64_t>(pixel), 2, sampleIndex, sample.green);
            deep.set_deep_value(static_cast<int64_t>(pixel), 3, sampleIndex, sample.blue);
            deep.set_deep_value(static_cast<int64_t>(pixel), 4, sampleIndex, sample.alpha);
        }
    }
    if (!output->open(utf8Path.constData(), spec) || !output->write_deep_image(deep)) {
        output->close();
        return false;
    }
    output->close();
    return true;
}

bool OpenExr::writeDeepRGBA32F(
    const QString& path, const DeepImageBuffer& image,
    const QString& compression) {
    if (image.isEmpty()) return false;
    const int width = image.width();
    const int height = image.height();
    const std::size_t pixelCount = static_cast<std::size_t>(width) *
                                   static_cast<std::size_t>(height);
    std::vector<std::vector<OpenExrDeepSample>> samples(pixelCount);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const DeepPixel* sourcePixel = image.pixel(x, y);
            if (!sourcePixel) return false;
            auto& destination = samples[static_cast<std::size_t>(y) * width + x];
            destination.reserve(sourcePixel->samples.size());
            for (const DeepSample& sample : sourcePixel->samples) {
                destination.push_back({sample.depth, sample.color[0], sample.color[1],
                                       sample.color[2], sample.alpha});
            }
        }
    }
    return writeDeepRGBA32F(path, width, height, samples, compression);
}

bool OpenExr::readDeepRGBA32F(
    const QString& path, std::vector<std::vector<OpenExrDeepSample>>& samples,
    int& width, int& height) const {
    samples.clear();
    width = height = 0;
    if (path.trimmed().isEmpty()) return false;
    const QByteArray utf8Path = QFileInfo(path).absoluteFilePath().toUtf8();
    auto input = OIIO::ImageInput::open(utf8Path.constData());
    if (!input) return false;
    const auto& spec = input->spec();
    if (!spec.deep || spec.width <= 0 || spec.height <= 0 || spec.nchannels < 4 ||
        spec.nchannels > 64) {
        input->close();
        return false;
    }
    const auto widthSize = static_cast<std::size_t>(spec.width);
    const auto heightSize = static_cast<std::size_t>(spec.height);
    if (widthSize > std::numeric_limits<std::size_t>::max() / heightSize) {
        input->close();
        return false;
    }
    const std::size_t pixelCount = widthSize * heightSize;
    OIIO::DeepData deep;
    if (!input->read_native_deep_image(0, 0, deep)) {
        input->close();
        return false;
    }
    std::array<int, 5> channelIndices{-1, -1, -1, -1, -1};
    const bool hasNamedChannels = !spec.channelnames.empty();
    for (int candidate = 0; candidate < spec.nchannels; ++candidate) {
        if (candidate >= static_cast<int>(spec.channelnames.size())) break;
        const QString name = QString::fromStdString(spec.channelnames[candidate]).toLower();
        const auto matches = [&name](const int channel) {
            if (channel == 0) return name == QLatin1String("z") ||
                name == QLatin1String("depth") || name.endsWith(QLatin1String(".z"));
            static constexpr const char* suffixes[] = {"r", "g", "b", "a"};
            const QString suffix = QString::fromLatin1(suffixes[channel - 1]);
            return name == suffix || name.endsWith(QStringLiteral(".") + suffix);
        };
        for (int channel = 0; channel < 5; ++channel) {
            if (channelIndices[channel] < 0 && matches(channel)) {
                channelIndices[channel] = candidate;
                break;
            }
        }
    }
    for (int channel = 0; channel < 5; ++channel) {
        if (!hasNamedChannels && channelIndices[channel] < 0 && channel < spec.nchannels)
            channelIndices[channel] = channel;
    }
    if (channelIndices[0] < 0) {
        input->close();
        return false;
    }
    samples.resize(pixelCount);
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        const int count = deep.samples(static_cast<int>(pixel));
        if (count < 0) {
            samples.clear();
            input->close();
            return false;
        }
        samples[pixel].resize(static_cast<std::size_t>(count));
        for (int sampleIndex = 0; sampleIndex < count; ++sampleIndex) {
            auto& sample = samples[pixel][static_cast<std::size_t>(sampleIndex)];
            sample.depth = channelIndices[0] >= 0
                ? deep.deep_value(static_cast<int64_t>(pixel), channelIndices[0], sampleIndex)
                : 0.0f;
            sample.red = channelIndices[1] >= 0
                ? deep.deep_value(static_cast<int64_t>(pixel), channelIndices[1], sampleIndex)
                : 0.0f;
            sample.green = channelIndices[2] >= 0
                ? deep.deep_value(static_cast<int64_t>(pixel), channelIndices[2], sampleIndex)
                : 0.0f;
            sample.blue = channelIndices[3] >= 0
                ? deep.deep_value(static_cast<int64_t>(pixel), channelIndices[3], sampleIndex)
                : 0.0f;
            sample.alpha = channelIndices[4] >= 0
                ? deep.deep_value(static_cast<int64_t>(pixel), channelIndices[4], sampleIndex)
                : 1.0f;
            if (!std::isfinite(sample.depth) || !std::isfinite(sample.red) ||
                !std::isfinite(sample.green) || !std::isfinite(sample.blue) ||
                !std::isfinite(sample.alpha)) {
                samples.clear();
                input->close();
                return false;
            }
        }
    }
    width = spec.width;
    height = spec.height;
    input->close();
    return true;
}

bool OpenExr::readDeepRGBA32F(
    const QString& path, DeepImageBuffer& image) const {
    std::vector<std::vector<OpenExrDeepSample>> samples;
    int width = 0;
    int height = 0;
    if (!readDeepRGBA32F(path, samples, width, height) ||
        !image.resize(width, height)) {
        image.resize(0, 0);
        return false;
    }
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto& sourcePixel = samples[static_cast<std::size_t>(y) * width + x];
            for (const OpenExrDeepSample& source : sourcePixel) {
                DeepSample destination;
                destination.depth = source.depth;
                destination.depthBack = source.depth;
                destination.color = {source.red, source.green, source.blue, source.alpha};
                destination.alpha = source.alpha;
                destination.coverage = 1.0f;
                if (!image.addSample(x, y, destination)) {
                    image.resize(0, 0);
                    return false;
                }
            }
        }
    }
    return image.normalizeSamples();
}

bool OpenExr::mergeDeepRGBA32F(
    const std::vector<std::vector<OpenExrDeepSample>>& front,
    const std::vector<std::vector<OpenExrDeepSample>>& back,
    std::vector<std::vector<OpenExrDeepSample>>& merged) {
    if (front.size() != back.size()) return false;
    auto sortedAndFinite = [](const auto& pixel) {
        return std::all_of(pixel.begin(), pixel.end(),
                           [](const OpenExrDeepSample& sample) {
                               return std::isfinite(sample.depth) &&
                                      std::isfinite(sample.red) &&
                                      std::isfinite(sample.green) &&
                                      std::isfinite(sample.blue) &&
                                      std::isfinite(sample.alpha);
                           }) &&
               std::is_sorted(pixel.begin(), pixel.end(),
                              [](const OpenExrDeepSample& lhs,
                                 const OpenExrDeepSample& rhs) {
                                  return lhs.depth <= rhs.depth;
                              });
    };
    for (const auto& pixel : front)
        if (!sortedAndFinite(pixel)) return false;
    for (const auto& pixel : back)
        if (!sortedAndFinite(pixel)) return false;

    merged.clear();
    merged.resize(front.size());
    for (std::size_t pixel = 0; pixel < front.size(); ++pixel) {
        auto& output = merged[pixel];
        output.reserve(front[pixel].size() + back[pixel].size());
        std::merge(front[pixel].begin(), front[pixel].end(),
                   back[pixel].begin(), back[pixel].end(),
                   std::back_inserter(output),
                   [](const OpenExrDeepSample& lhs,
                      const OpenExrDeepSample& rhs) {
                       return lhs.depth < rhs.depth;
                   });
    }
    return true;
}

bool OpenExr::flattenDeepRGBA32F(
    const std::vector<std::vector<OpenExrDeepSample>>& samples,
    int width, int height, std::vector<float>& rgba) {
    rgba.clear();
    if (width <= 0 || height <= 0) return false;
    const auto widthSize = static_cast<std::size_t>(width);
    const auto heightSize = static_cast<std::size_t>(height);
    if (widthSize > std::numeric_limits<std::size_t>::max() / heightSize)
        return false;
    const std::size_t pixelCount = widthSize * heightSize;
    if (samples.size() != pixelCount ||
        pixelCount > std::numeric_limits<std::size_t>::max() / 4u)
        return false;
    rgba.assign(pixelCount * 4u, 0.0f);
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        float* output = rgba.data() + pixel * 4u;
        float previousDepth = -std::numeric_limits<float>::infinity();
        for (const auto& sample : samples[pixel]) {
            if (!std::isfinite(sample.depth) || !std::isfinite(sample.red) ||
                !std::isfinite(sample.green) || !std::isfinite(sample.blue) ||
                !std::isfinite(sample.alpha) || sample.depth < previousDepth) {
                rgba.clear();
                return false;
            }
            previousDepth = sample.depth;
            const float alpha = std::clamp(sample.alpha, 0.0f, 1.0f);
            const float visibility = 1.0f - output[3];
            output[0] += sample.red * visibility;
            output[1] += sample.green * visibility;
            output[2] += sample.blue * visibility;
            output[3] += alpha * visibility;
            if (output[3] >= 1.0f - 1.0e-6f) {
                output[3] = 1.0f;
                break;
            }
        }
    }
    return true;
}

} // namespace ArtifactCore
