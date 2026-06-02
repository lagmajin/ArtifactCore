module;
#include <algorithm>
#include <cmath>
#include <memory>

#include <QImage>
#include <QPointF>
#include <QRect>
#include <QString>
#include <QSize>
#include <opencv2/opencv.hpp>

export module Tracking.PointTracker;

import Image.ImageF32x4_RGBA;

export namespace ArtifactCore::Tracking {

struct PointTrackerConfig {
  int templateSize = 31;
  int searchSize = 71;
  bool subPixel = true;
};

struct PointTrackResult {
  QPointF position;
  float confidence = 0.0f;
  bool success = false;
};

class PointTracker {
public:
  PointTracker();
  ~PointTracker();

  void clear();
  bool isReady() const;

  void setTemplate(const ArtifactCore::ImageF32x4_RGBA &frame,
                   const QPointF &center,
                   const PointTrackerConfig &config = PointTrackerConfig{});

  PointTrackResult track(const ArtifactCore::ImageF32x4_RGBA &nextFrame);

  QPointF templateCenter() const;
  PointTrackerConfig config() const;

private:
  struct Impl;
  Impl *impl_;
};

namespace {
cv::Mat toGrayMat(const QImage &image) {
  if (image.isNull()) {
    return {};
  }
  const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
  return cv::Mat(gray.height(), gray.width(), CV_8UC1,
                 const_cast<uchar *>(gray.constBits()), gray.bytesPerLine())
      .clone();
}

QRect clampCenteredSquare(const QPointF &center, int size, const QSize &bounds) {
  const int half = std::max(1, size) / 2;
  int x = static_cast<int>(std::lround(center.x())) - half;
  int y = static_cast<int>(std::lround(center.y())) - half;
  int w = std::max(1, size);
  int h = std::max(1, size);
  if (bounds.width() <= 0 || bounds.height() <= 0) {
    return {};
  }
  if (w > bounds.width()) {
    w = bounds.width();
    x = 0;
  }
  if (h > bounds.height()) {
    h = bounds.height();
    y = 0;
  }
  x = std::clamp(x, 0, std::max(0, bounds.width() - w));
  y = std::clamp(y, 0, std::max(0, bounds.height() - h));
  return QRect(x, y, w, h);
}

float parabolicSubPixel(const cv::Mat &response, const cv::Point &peak, bool horizontal) {
  if (response.empty() || peak.x <= 0 || peak.y <= 0 ||
      peak.x >= response.cols - 1 || peak.y >= response.rows - 1) {
    return 0.0f;
  }

  const int x = peak.x;
  const int y = peak.y;
  const float c = response.at<float>(y, x);
  const float l = horizontal ? response.at<float>(y, x - 1) : response.at<float>(y - 1, x);
  const float r = horizontal ? response.at<float>(y, x + 1) : response.at<float>(y + 1, x);
  const float denom = (l - 2.0f * c + r);
  if (std::abs(denom) < 1e-6f) {
    return 0.0f;
  }
  const float offset = 0.5f * (l - r) / denom;
  return std::clamp(offset, -1.0f, 1.0f);
}
} // namespace

struct PointTracker::Impl {
  cv::Mat templateGray;
  QPointF center;
  PointTrackerConfig config;
  QSize frameSize;
  bool ready = false;
};

PointTracker::PointTracker() : impl_(new Impl()) {}
PointTracker::~PointTracker() { delete impl_; }

void PointTracker::clear() {
  if (!impl_) {
    return;
  }
  impl_->templateGray.release();
  impl_->center = {};
  impl_->frameSize = {};
  impl_->config = {};
  impl_->ready = false;
}

bool PointTracker::isReady() const {
  return impl_ && impl_->ready && !impl_->templateGray.empty();
}

void PointTracker::setTemplate(const ArtifactCore::ImageF32x4_RGBA &frame,
                               const QPointF &center,
                               const PointTrackerConfig &config) {
  if (!impl_) {
    return;
  }

  const QImage image = frame.toQImage();
  const cv::Mat gray = toGrayMat(image);
  if (gray.empty()) {
    clear();
    return;
  }

  impl_->config = config;
  impl_->config.templateSize = std::max(5, impl_->config.templateSize | 1);
  impl_->config.searchSize = std::max(impl_->config.templateSize + 4, impl_->config.searchSize | 1);

  const QRect templateRect =
      clampCenteredSquare(center, impl_->config.templateSize, QSize(gray.cols, gray.rows));
  if (templateRect.isEmpty() || templateRect.width() <= 1 || templateRect.height() <= 1) {
    clear();
    return;
  }

  impl_->templateGray = gray(cv::Rect(templateRect.x(), templateRect.y(),
                                      templateRect.width(), templateRect.height()))
                            .clone();
  impl_->center = center;
  impl_->frameSize = QSize(gray.cols, gray.rows);
  impl_->ready = !impl_->templateGray.empty();
}

PointTrackResult PointTracker::track(const ArtifactCore::ImageF32x4_RGBA &nextFrame) {
  PointTrackResult result;
  if (!isReady()) {
    return result;
  }

  const QImage image = nextFrame.toQImage();
  const cv::Mat gray = toGrayMat(image);
  if (gray.empty()) {
    return result;
  }

  const QRect searchRect =
      clampCenteredSquare(impl_->center, impl_->config.searchSize, QSize(gray.cols, gray.rows));
  if (searchRect.isEmpty() || searchRect.width() < impl_->templateGray.cols ||
      searchRect.height() < impl_->templateGray.rows) {
    return result;
  }

  const cv::Rect roi(searchRect.x(), searchRect.y(), searchRect.width(), searchRect.height());
  const cv::Mat searchGray = gray(roi);
  cv::Mat response;
  cv::matchTemplate(searchGray, impl_->templateGray, response, cv::TM_CCOEFF_NORMED);
  if (response.empty()) {
    return result;
  }

  double minVal = 0.0;
  double maxVal = 0.0;
  cv::Point minLoc;
  cv::Point maxLoc;
  cv::minMaxLoc(response, &minVal, &maxVal, &minLoc, &maxLoc);

  const float subX = impl_->config.subPixel ? parabolicSubPixel(response, maxLoc, true) : 0.0f;
  const float subY = impl_->config.subPixel ? parabolicSubPixel(response, maxLoc, false) : 0.0f;

  const QPointF nextCenter(
      searchRect.left() + static_cast<double>(maxLoc.x) + subX +
          static_cast<double>(impl_->templateGray.cols) * 0.5,
      searchRect.top() + static_cast<double>(maxLoc.y) + subY +
          static_cast<double>(impl_->templateGray.rows) * 0.5);

  impl_->center = nextCenter;
  result.position = nextCenter;
  result.confidence = static_cast<float>(std::clamp(maxVal, 0.0, 1.0));
  result.success = result.confidence >= 0.15f;
  return result;
}

QPointF PointTracker::templateCenter() const {
  return impl_ ? impl_->center : QPointF{};
}

PointTrackerConfig PointTracker::config() const {
  return impl_ ? impl_->config : PointTrackerConfig{};
}

} // namespace ArtifactCore::Tracking
