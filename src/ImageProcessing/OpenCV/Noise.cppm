module;
#include <utility>
#include <random>
#include <cmath>
#include <vector>
#include <opencv2/opencv.hpp>
module Noise;
import Noise;

import Core.Parallel;

namespace ArtifactCore {

cv::Mat addNoise(const cv::Mat& input, NoiseType type, float amount, bool monochrome, int seed) {
    if (input.empty()) return input;

    cv::Mat src;
    bool wasFloat = (input.depth() == CV_32F);
    if (!wasFloat) {
        input.convertTo(src, CV_32F, 1.0 / 255.0);
    } else {
        src = input.clone();
    }

    cv::Mat result = src.clone();
    int ch = src.channels();

    std::mt19937 rng(seed >= 0 ? seed : std::random_device{}());
    std::normal_distribution<float> gaussian(0.0f, amount);
    std::uniform_real_distribution<float> uniform(-amount, amount);

    switch (type) {
        case NoiseType::Gaussian: {
            if (monochrome) {
                cv::Mat noiseMono(src.size(), CV_32F);
                for (int y = 0; y < src.rows; ++y) {
                    float* noiseRow = noiseMono.ptr<float>(y);
                    for (int x = 0; x < src.cols; ++x) {
                        noiseRow[x] = gaussian(rng);
                    }
                }
                Parallel::For(0, src.rows, src.rows * src.cols, [&](int y) {
                    const float* noiseRow = noiseMono.ptr<float>(y);
                    float* resultRow = result.ptr<float>(y);
                    for (int x = 0; x < src.cols; ++x) {
                        const float n = noiseRow[x];
                        for (int c = 0; c < std::min(ch, 3); ++c) {
                            resultRow[x * ch + c] += n;
                        }
                    }
                });
            } else {
                cv::Mat noise(src.size(), src.type());
                cv::randn(noise, 0.0, amount);
                result += noise;
            }
            break;
        }
        case NoiseType::Uniform: {
            cv::Mat noise(src.size(), src.type());
            cv::randu(noise, -amount, amount);
            result += noise;
            break;
        }
        case NoiseType::SaltAndPepper: {
            float prob = amount * 0.05f;
            std::vector<float> randomValues(static_cast<size_t>(src.rows) * static_cast<size_t>(src.cols));
            for (float& value : randomValues) {
                value = uniform(rng) + amount;
            }
            Parallel::For(0, src.rows, src.rows * src.cols, [&](int y) {
                float* resultRow = result.ptr<float>(y);
                for (int x = 0; x < src.cols; ++x) {
                    const float r = randomValues[static_cast<size_t>(y) * static_cast<size_t>(src.cols) + static_cast<size_t>(x)];
                    if (r < prob) {
                        for (int c = 0; c < std::min(ch, 3); ++c)
                            resultRow[x * ch + c] = 0.0f;
                    } else if (r > (2.0f * amount - prob)) {
                        for (int c = 0; c < std::min(ch, 3); ++c)
                            resultRow[x * ch + c] = 1.0f;
                    }
                }
            });
            break;
        }
        case NoiseType::Perlin: {
            // Simplified Perlin-like noise using multi-scale Gaussian blur
            cv::Mat noise = cv::Mat::zeros(src.size(), CV_32F);
            float scale = 1.0f;
            for (int octave = 0; octave < 4; ++octave) {
                cv::Mat octaveNoise(src.size(), CV_32F);
                cv::randn(octaveNoise, 0.0, amount * scale);
                int blur = (1 << (4 - octave)) * 2 + 1;
                cv::GaussianBlur(octaveNoise, octaveNoise, cv::Size(blur, blur), 0);
                noise += octaveNoise;
                scale *= 0.5f;
            }
            std::vector<float> randomValues;
            if (!monochrome) {
                randomValues.resize(static_cast<size_t>(src.rows) *
                                    static_cast<size_t>(src.cols) *
                                    static_cast<size_t>(ch));
                for (float& value : randomValues) {
                    value = uniform(rng);
                }
            }
            Parallel::For(0, src.rows, src.rows * src.cols, [&](int y) {
                float* resultRow = result.ptr<float>(y);
                const float* noiseRow = noise.ptr<float>(y);
                for (int x = 0; x < src.cols; ++x) {
                    const float n = noiseRow[x];
                    if (monochrome) {
                        for (int c = 0; c < std::min(ch, 3); ++c)
                            resultRow[x * ch + c] += n;
                    } else {
                        const size_t randomBase =
                            (static_cast<size_t>(y) * static_cast<size_t>(src.cols) +
                             static_cast<size_t>(x)) * static_cast<size_t>(ch);
                        for (int c = 0; c < ch; ++c)
                            resultRow[x * ch + c] += n * (0.8f + 0.4f * randomValues[randomBase + static_cast<size_t>(c)] / amount);
                    }
                }
            });
            break;
        }
        case NoiseType::FilmGrain: {
            // Film grain: Gaussian noise with luminance-dependent intensity
            cv::Mat gray;
            if (ch >= 3) {
                cv::cvtColor(src, gray, (ch == 4) ? cv::COLOR_BGRA2GRAY : cv::COLOR_BGR2GRAY);
            } else {
                gray = src;
            }

            const size_t pixelCount = static_cast<size_t>(src.rows) * static_cast<size_t>(src.cols);
            const size_t channelCount = static_cast<size_t>(std::min(ch, 3));
            const size_t randomStride = monochrome ? 1ull : 1ull + channelCount;
            std::vector<float> randomValues(pixelCount * randomStride);
            for (size_t pixel = 0; pixel < pixelCount; ++pixel) {
                randomValues[pixel * randomStride] = gaussian(rng);
                if (!monochrome) {
                    for (size_t c = 0; c < channelCount; ++c) {
                        randomValues[pixel * randomStride + 1ull + c] = gaussian(rng);
                    }
                }
            }
            Parallel::For(0, src.rows, src.rows * src.cols, [&](int y) {
                float* resultRow = result.ptr<float>(y);
                const float* grayRow = gray.ptr<float>(y);
                for (int x = 0; x < src.cols; ++x) {
                    const size_t pixel = static_cast<size_t>(y) * static_cast<size_t>(src.cols) + static_cast<size_t>(x);
                    const float lum = grayRow[x];
                    // More grain in midtones, less in shadows and highlights
                    const float grainAmount = amount * 4.0f * lum * (1.0f - lum);
                    const float n = randomValues[pixel * randomStride] * grainAmount;

                    if (monochrome) {
                        for (int c = 0; c < std::min(ch, 3); ++c)
                            resultRow[x * ch + c] += n;
                    } else {
                        for (size_t c = 0; c < channelCount; ++c)
                            resultRow[x * ch + static_cast<int>(c)] +=
                                randomValues[pixel * randomStride + 1ull + c] * grainAmount;
                    }
                }
            });
            break;
        }
    }

    cv::min(result, 1.0f, result);
    cv::max(result, 0.0f, result);

    if (!wasFloat) {
        result.convertTo(result, input.depth(), 255.0);
    }

    return result;
}

cv::Mat generateNoiseTexture(int width, int height, NoiseType type, float amount, int seed) {
    cv::Mat blank = cv::Mat::zeros(height, width, CV_32FC3) + 0.5f;
    return addNoise(blank, type, amount, true, seed);
}

} // namespace ArtifactCore
