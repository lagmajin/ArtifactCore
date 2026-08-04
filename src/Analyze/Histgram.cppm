module;
#include <opencv2/opencv.hpp>
#include <vector>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Analyze.Histgram;

namespace ArtifactCore {

void Histgram::Impl::calculate(const cv::Mat& image) {
    bins_.clear();
    if (image.empty()) return;

    int channels = image.channels();
    bins_.resize(channels, std::vector<int>(histSize_, 0));

    if (image.depth() != CV_8U && image.depth() != CV_16U &&
        image.depth() != CV_32F && image.depth() != CV_64F) {
        bins_.clear();
        return;
    }

    // Calculate histogram for each channel. Values are normalized to the
    // public 256-bin range for all supported scalar image depths.
    for (int ch = 0; ch < channels; ++ch) {
        std::vector<int> hist(histSize_, 0);
        for (int y = 0; y < image.rows; ++y) {
            for (int x = 0; x < image.cols; ++x) {
                const int index = x * channels + ch;
                int value = 0;
                switch (image.depth()) {
                case CV_8U:
                    value = image.ptr<uchar>(y)[index];
                    break;
                case CV_16U:
                    value = static_cast<int>(image.ptr<unsigned short>(y)[index] >> 8);
                    break;
                case CV_32F: {
                    const float sample = image.ptr<float>(y)[index];
                    value = static_cast<int>(std::lround(
                        std::clamp(sample, 0.0f, 1.0f) * 255.0f));
                    break;
                }
                case CV_64F: {
                    const double sample = image.ptr<double>(y)[index];
                    value = static_cast<int>(std::lround(
                        std::clamp(sample, 0.0, 1.0) * 255.0));
                    break;
                }
                default:
                    break;
                }
                ++hist[value];
            }
        }
        bins_[ch] = hist;
    }
}

std::vector<int> Histgram::Impl::getBins(int channel) const {
    if (channel >= 0 && channel < static_cast<int>(bins_.size())) {
        return bins_[channel];
    }
    return {};
}

int Histgram::Impl::getMaxValue(int channel) const {
    if (channel >= 0 && channel < static_cast<int>(bins_.size())) {
        const auto& hist = bins_[channel];
        return *std::max_element(hist.begin(), hist.end());
    }
    return 0;
}

int Histgram::Impl::getChannelCount() const {
    return bins_.size();
}

void Histgram::Impl::normalize() {
    for (auto& hist : bins_) {
        int maxVal = *std::max_element(hist.begin(), hist.end());
        if (maxVal > 0) {
            for (int& val : hist) {
                val = (val * 255) / maxVal; // Scale to 0-255
            }
        }
    }
}

void Histgram::Impl::clear() {
    bins_.clear();
}

Histgram::Histgram() : impl_(new Impl()) {}

Histgram::~Histgram() {
    delete impl_;
}

void Histgram::calculate(const cv::Mat& image) {
    impl_->calculate(image);
}

std::vector<int> Histgram::getBins(int channel) const {
    return impl_->getBins(channel);
}

int Histgram::getMaxValue(int channel) const {
    return impl_->getMaxValue(channel);
}

int Histgram::getChannelCount() const {
    return impl_->getChannelCount();
}

void Histgram::normalize() {
    impl_->normalize();
}

void Histgram::clear() {
    impl_->clear();
}

};
