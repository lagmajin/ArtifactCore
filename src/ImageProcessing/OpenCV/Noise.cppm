module;
#include <utility>
#include <random>
#include <cmath>
#include <opencv2/opencv.hpp>
module Noise;

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
                    for (int x = 0; x < src.cols; ++x) {
                        noiseMono.at<float>(y, x) = gaussian(rng);
                    }
                }
                for (int y = 0; y < src.rows; ++y) {
                    for (int x = 0; x < src.cols; ++x) {
                        float n = noiseMono.at<float>(y, x);
                        for (int c = 0; c < std::min(ch, 3); ++c) {
                            result.ptr<float>(y)[x * ch + c] += n;
                        }
                    }
                }
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
            for (int y = 0; y < src.rows; ++y) {
                for (int x = 0; x < src.cols; ++x) {
                    float r = uniform(rng) + amount;
                    if (r < prob) {
                        for (int c = 0; c < std::min(ch, 3); ++c)
                            result.ptr<float>(y)[x * ch + c] = 0.0f;
                    } else if (r > (2.0f * amount - prob)) {
                        for (int c = 0; c < std::min(ch, 3); ++c)
                            result.ptr<float>(y)[x * ch + c] = 1.0f;
                    }
                }
            }
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
            for (int y = 0; y < src.rows; ++y) {
                for (int x = 0; x < src.cols; ++x) {
                    float n = noise.at<float>(y, x);
                    if (monochrome) {
                        for (int c = 0; c < std::min(ch, 3); ++c)
                            result.ptr<float>(y)[x * ch + c] += n;
                    } else {
                        for (int c = 0; c < ch; ++c)
                            result.ptr<float>(y)[x * ch + c] += n * (0.8f + 0.4f * uniform(rng) / amount);
                    }
                }
            }
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

            for (int y = 0; y < src.rows; ++y) {
                for (int x = 0; x < src.cols; ++x) {
                    float lum = gray.at<float>(y, x);
                    // More grain in midtones, less in shadows and highlights
                    float grainAmount = amount * 4.0f * lum * (1.0f - lum);
                    float n = gaussian(rng) * grainAmount;

                    if (monochrome) {
                        for (int c = 0; c < std::min(ch, 3); ++c)
                            result.ptr<float>(y)[x * ch + c] += n;
                    } else {
                        for (int c = 0; c < std::min(ch, 3); ++c)
                            result.ptr<float>(y)[x * ch + c] += gaussian(rng) * grainAmount;
                    }
                }
            }
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
