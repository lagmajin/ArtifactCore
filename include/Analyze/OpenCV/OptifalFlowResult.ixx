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
export module Analyze.OpticalFlow;





export namespace ArtifactCore {

    class OpticalFlowResult {
    private:
        cv::Mat flow; // CV_32FC2

    public:
        OpticalFlowResult(const cv::Mat& flow_) : flow(flow_.clone()) {}
        ~OpticalFlowResult() = default;

        // 平均動きベクトルを計算
        cv::Point2f getAverageFlow() const {
            if (flow.empty()) return cv::Point2f(0.0f, 0.0f);
            
            cv::Scalar meanFlow = cv::mean(flow);
            return cv::Point2f(static_cast<float>(meanFlow[0]), static_cast<float>(meanFlow[1]));
        }

        // 動きの大きさの閾値以上の画素をマスクで返す
        cv::Mat getMagnitudeMask(float threshold) const {
            if (flow.empty()) return cv::Mat();

            std::vector<cv::Mat> channels(2);
            cv::split(flow, channels);

            cv::Mat magnitude, angle;
            cv::cartToPolar(channels[0], channels[1], magnitude, angle);

            cv::Mat mask;
            cv::threshold(magnitude, mask, threshold, 255, cv::THRESH_BINARY);
            mask.convertTo(mask, CV_8UC1);
            return mask;
        }

        // 方向ヒストグラム（8方向）
        std::vector<int> getDirectionHistogram(int bins = 8) const {
            std::vector<int> hist(bins, 0);
            if (flow.empty()) return hist;

            std::vector<cv::Mat> channels(2);
            cv::split(flow, channels);

            cv::Mat magnitude, angle;
            cv::cartToPolar(channels[0], channels[1], magnitude, angle, true); // angle in degrees

            for (int y = 0; y < angle.rows; ++y) {
                for (int x = 0; x < angle.cols; ++x) {
                    float a = angle.at<float>(y, x);
                    float m = magnitude.at<float>(y, x);
                    
                    if (m > 0.5f) { // 動きが小さいものは無視
                        int bin = static_cast<int>(a / (360.0f / bins)) % bins;
                        hist[bin]++;
                    }
                }
            }
            return hist;
        }

        // 動きベクトルを色で可視化した画像を作成
        cv::Mat visualizeFlow() const {
            if (flow.empty()) return cv::Mat();

            std::vector<cv::Mat> channels(2);
            cv::split(flow, channels);

            cv::Mat magnitude, angle;
            cv::cartToPolar(channels[0], channels[1], magnitude, angle, true);

            cv::Mat hsv(flow.size(), CV_32FC3);
            std::vector<cv::Mat> hsv_channels(3);
            
            // Hue: Angle (0-360 mapped to 0-180 for OpenCV HSV)
            hsv_channels[0] = angle / 2.0f;
            // Saturation: 255
            hsv_channels[1] = cv::Mat::ones(flow.size(), CV_32F) * 255.0f;
            // Value: Magnitude, normalized
            cv::normalize(magnitude, hsv_channels[2], 0, 255, cv::NORM_MINMAX);

            cv::merge(hsv_channels, hsv);

            cv::Mat hsv8, bgr;
            hsv.convertTo(hsv8, CV_8UC3);
            cv::cvtColor(hsv8, bgr, cv::COLOR_HSV2BGR);

            return bgr;
        }
        
        const cv::Mat& getFlowMat() const {
            return flow;
        }
    };

    // ─────────────────────────────────────────────────────────
    // OpticalFlowEngine
    // 2枚の画像からFarnebackアルゴリズムを用いてOptical Flowを計算する
    // ─────────────────────────────────────────────────────────
    class OpticalFlowEngine {
    public:
        // パラメータ
        double pyr_scale = 0.5;
        int levels = 3;
        int winsize = 15;
        int iterations = 3;
        int poly_n = 5;
        double poly_sigma = 1.2;
        int flags = 0;

        OpticalFlowResult compute(const cv::Mat& prevFrame, const cv::Mat& nextFrame) const {
            cv::Mat prevGray, nextGray;
            
            if (prevFrame.channels() == 3 || prevFrame.channels() == 4) {
                cv::cvtColor(prevFrame, prevGray, cv::COLOR_BGR2GRAY);
            } else {
                prevGray = prevFrame;
            }

            if (nextFrame.channels() == 3 || nextFrame.channels() == 4) {
                cv::cvtColor(nextFrame, nextGray, cv::COLOR_BGR2GRAY);
            } else {
                nextGray = nextFrame;
            }

            cv::Mat flow;
            cv::calcOpticalFlowFarneback(
                prevGray, nextGray, flow, 
                pyr_scale, levels, winsize, iterations, poly_n, poly_sigma, flags
            );

            return OpticalFlowResult(flow);
        }
    };

}
