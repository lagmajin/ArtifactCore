module Analyze.Histgram;

#include <opencv2/opencv.hpp>
#include <vector>

import std;

namespace ArtifactCore {

void Histgram::Impl::calculate(const cv::Mat& image) {
    bins_.clear();
    if (image.empty()) return;

    int channels = image.channels();
    bins_.resize(channels, std::vector<int>(histSize_, 0));

    // Calculate histogram for each channel
    for (int ch = 0; ch < channels; ++ch) {
        std::vector<int> hist(histSize_, 0);
        for (int y = 0; y < image.rows; ++y) {
            for (int x = 0; x < image.cols; ++x) {
                int value = 0;
                if (channels == 1) {
                    value = image.at<uchar>(y, x);
                } else if (channels == 3) {
                    cv::Vec3b pixel = image.at<cv::Vec3b>(y, x);
                    value = pixel[ch];
                } else if (channels == 4) {
                    cv::Vec4b pixel = image.at<cv::Vec4b>(y, x);
                    value = pixel[ch];
                }
                if (value >= 0 && value < histSize_) {
                    hist[value]++;
                }
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