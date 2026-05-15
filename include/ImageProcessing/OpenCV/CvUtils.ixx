module;
#include <utility>
#include <QImage>
#include <QColor>
#include <vector>

#include <opencv2/opencv.hpp>
export module CvUtils;

export namespace ArtifactCore {

/**
 * @brief Utilities for converting between Qt and OpenCV image formats
 */
namespace CvUtils {

    /**
     * @brief Convert QImage to cv::Mat
     * @param image Input QImage
     * @param cloneData If true, ensures a deep copy. If false, might return a mat sharing the same memory (if possible).
     * @return cv::Mat containing the image data. Format depends on image format.
     */
    inline cv::Mat qImageToCvMat(const QImage& image, bool cloneData = true) {
        cv::Mat mat;
        switch (image.format()) {
            case QImage::Format_ARGB32:
            case QImage::Format_ARGB32_Premultiplied:
            case QImage::Format_RGB32:
                mat = cv::Mat(image.height(), image.width(), CV_8UC4, (void*)image.constBits(), image.bytesPerLine());
                break;
            case QImage::Format_RGB888:
                // Note: QImage is RGB, OpenCV is BGR by default
                mat = cv::Mat(image.height(), image.width(), CV_8UC3, (void*)image.constBits(), image.bytesPerLine());
                cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
                return mat; // cvtColor always makes a copy
            case QImage::Format_Grayscale8:
            case QImage::Format_Indexed8:
                mat = cv::Mat(image.height(), image.width(), CV_8UC1, (void*)image.constBits(), image.bytesPerLine());
                break;
            default:
                // For other formats, convert to ARGB32 first
                return qImageToCvMat(image.convertToFormat(QImage::Format_ARGB32), true);
        }
        return cloneData ? mat.clone() : mat;
    }

    /**
     * @brief Convert cv::Mat to QImage
     * @param mat Input cv::Mat (supports 8-bit Gray/BGR/BGRA and 32-bit float Gray/BGR/BGRA)
     * @return QImage Representation of the mat
     */
    inline QImage cvMatToQImage(const cv::Mat& mat) {
        if (mat.empty()) return QImage();

        switch (mat.type()) {
            case CV_8UC4: {
                // Assuming BGRA (OpenCV default) -> ARGB32 (Qt default on Little Endian)
                // Actually Format_ARGB32 in Qt is BGRA on Little Endian.
                QImage image(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_ARGB32);
                return image.copy();
            }
            case CV_8UC3: {
                cv::Mat rgb;
                cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
                QImage image(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
                return image.copy();
            }
            case CV_8UC1: {
                QImage image(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_Grayscale8);
                return image.copy();
            }
            case CV_32FC4: {
                cv::Mat u8;
                mat.convertTo(u8, CV_8U, 255.0);
                return cvMatToQImage(u8);
            }
            case CV_32FC3: {
                cv::Mat u8;
                mat.convertTo(u8, CV_8U, 255.0);
                return cvMatToQImage(u8);
            }
            case CV_32FC1: {
                cv::Mat u8;
                mat.convertTo(u8, CV_8U, 255.0);
                return cvMatToQImage(u8);
            }
            default:
                return QImage();
        }
    }

    /**
     * @brief Scale Mat to fit a certain size while keeping aspect ratio
     */
    inline cv::Mat resizeKeepAspect(const cv::Mat& src, int width, int height) {
        if (src.empty()) return src;
        
        double scale = std::min(static_cast<double>(width) / src.cols, static_cast<double>(height) / src.rows);
        cv::Mat dst;
        cv::resize(src, dst, cv::Size(), scale, scale, cv::INTER_LINEAR);
        return dst;
    }

    /**
     * @brief Ensure the Mat is in BGR format (3 channels)
     */
    inline cv::Mat ensureBGR(const cv::Mat& src) {
        if (src.empty() || src.channels() == 3) return src;
        cv::Mat dst;
        if (src.channels() == 4) {
            cv::cvtColor(src, dst, cv::COLOR_BGRA2BGR);
        } else if (src.channels() == 1) {
            cv::cvtColor(src, dst, cv::COLOR_GRAY2BGR);
        }
        return dst;
    }

    /**
     * @brief Ensure the Mat is Gray (1 channel)
     */
    inline cv::Mat ensureGray(const cv::Mat& src) {
        if (src.empty() || src.channels() == 1) return src;
        cv::Mat dst;
        if (src.channels() == 3) {
            cv::cvtColor(src, dst, cv::COLOR_BGR2GRAY);
        } else if (src.channels() == 4) {
            cv::cvtColor(src, dst, cv::COLOR_BGRA2GRAY);
        }
        return dst;
    }

    /**
     * @brief Draw a rectangle with text (for detection visualization)
     */
    inline void drawDetection(cv::Mat& img, const cv::Rect& rect, const std::string& label, const cv::Scalar& color) {
        cv::rectangle(img, rect, color, 2);
        
        int baseLine;
        cv::Size labelSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);
        
        int top = std::max(rect.y, labelSize.height);
        cv::rectangle(img, cv::Point(rect.x, top - labelSize.height),
                      cv::Point(rect.x + labelSize.width, top + baseLine), color, cv::FILLED);
        cv::putText(img, label, cv::Point(rect.x, top), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
    }
}

} // namespace ArtifactCore
