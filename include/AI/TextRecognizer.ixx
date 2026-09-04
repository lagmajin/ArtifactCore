module;
#include <algorithm>
#include <cmath>
#include <QList>
#include <QRect>
#include <QString>
#include <QStringView>

export module Core.AI.TextRecognizer;

import Image.DepthMap;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

struct RecognizedTextRegion {
  QString text;
  float confidence = 0.0f;
  QRect rect;
  QString language;
};

class ITextRecognizer {
public:
  virtual ~ITextRecognizer() = default;
  virtual bool isReady() const noexcept = 0;
  virtual QList<RecognizedTextRegion> recognize(const ImageF32x4_RGBA& image) = 0;
  virtual QString lastError() const = 0;
};

inline QList<RecognizedTextRegion> filterRecognizedText(
    const QList<RecognizedTextRegion>& regions, QStringView query = {},
    float minimumConfidence = 0.0f) {
  QList<RecognizedTextRegion> filtered;
  const float threshold = std::clamp(minimumConfidence, 0.0f, 1.0f);
  for (const auto& region : regions) {
    if (region.confidence < threshold ||
        (!query.isEmpty() && !region.text.contains(query, Qt::CaseInsensitive))) continue;
    filtered.append(region);
  }
  return filtered;
}

inline QString joinRecognizedText(const QList<RecognizedTextRegion>& regions,
                                  const QString& separator = QStringLiteral("\n")) {
  QString text;
  for (const auto& region : regions) {
    if (region.text.isEmpty()) continue;
    if (!text.isEmpty()) text += separator;
    text += region.text;
  }
  return text;
}

inline QList<RecognizedTextRegion> sortRecognizedTextReadingOrder(
    QList<RecognizedTextRegion> regions, int lineTolerancePixels = 8) {
  const int tolerance = std::max(lineTolerancePixels, 0);
  std::stable_sort(regions.begin(), regions.end(), [tolerance](const auto& left, const auto& right) {
    if (std::abs(left.rect.top() - right.rect.top()) <= tolerance) return left.rect.left() < right.rect.left();
    return left.rect.top() < right.rect.top();
  });
  return regions;
}

inline QRect recognizedTextBounds(const QList<RecognizedTextRegion>& regions,
                                  float minimumConfidence = 0.0f) {
  const float threshold = std::clamp(minimumConfidence, 0.0f, 1.0f);
  QRect bounds;
  for (const auto& region : regions) {
    if (region.confidence < threshold || region.rect.isEmpty()) continue;
    bounds = bounds.isNull() ? region.rect : bounds.united(region.rect);
  }
  return bounds;
}

inline bool rasterizeTextRegionsMask(const QList<RecognizedTextRegion>& regions,
                                     int width, int height, DepthMap& mask,
                                     float minimumConfidence = 0.0f,
                                     int paddingPixels = 0) {
  if (width <= 0 || height <= 0) { mask.clear(); return false; }
  mask.resize(width, height);
  const float threshold = std::clamp(minimumConfidence, 0.0f, 1.0f);
  const int padding = std::max(paddingPixels, 0);
  const QRect canvas(0, 0, width, height);
  for (const auto& region : regions) {
    if (region.confidence < threshold || region.rect.isEmpty()) { continue; }
    const QRect rect = region.rect.adjusted(-padding, -padding, padding, padding).intersected(canvas);
    for (int y = rect.top(); y <= rect.bottom(); ++y)
      for (int x = rect.left(); x <= rect.right(); ++x) mask.setValue(x, y, 1.0f);
  }
  return true;
}

} // namespace ArtifactCore
