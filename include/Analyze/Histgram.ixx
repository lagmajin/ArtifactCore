module;
#include <opencv2/opencv.hpp>
#include <vector>
#include <iostream>

export module Analyze.Histgram;

import std;

export namespace ArtifactCore {

class Histgram {
private:
    struct Impl {
        std::vector<std::vector<int>> bins_; // bins_[channel][bin]
        int histSize_ = 256; // Number of bins

        void calculate(const cv::Mat& image);
        std::vector<int> getBins(int channel) const;
        int getMaxValue(int channel) const;
        int getChannelCount() const;
        void normalize();
        void clear();
    };
    Impl* impl_;
public:
    Histgram();
    ~Histgram();

    // Calculate histogram from image
    void calculate(const cv::Mat& image);

    // Get histogram bins for a channel
    std::vector<int> getBins(int channel) const;

    // Get maximum value in a channel
    int getMaxValue(int channel) const;

    // Get number of channels
    int getChannelCount() const;

    // Normalize histogram
    void normalize();

    // Clear histogram
    void clear();
};

};