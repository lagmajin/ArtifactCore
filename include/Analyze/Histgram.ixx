module;
#include <vector>
#include <iostream>

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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <opencv2/opencv.hpp>
export module Analyze.Histgram;

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
