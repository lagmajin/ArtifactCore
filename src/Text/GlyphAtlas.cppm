module;
#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QRawFont>
#include <QString>
#include <algorithm>
#include <cstdint>
#include <cstring>


module Text.GlyphAtlas;

import Text.Style;
import Font.FreeFont;

namespace ArtifactCore {

// ---------------------------------------------------------------------------
// GlyphAtlas
// ---------------------------------------------------------------------------

GlyphAtlas::GlyphAtlas()
    : atlasImage_(kAtlasSize, kAtlasSize, QImage::Format_RGBA8888) {
  atlasImage_.fill(Qt::transparent);
  dirty_ = true;
}

GlyphAtlas::~GlyphAtlas() = default;

void GlyphAtlas::clear() {
  cache_.clear();
  atlasImage_.fill(Qt::transparent);
  currentShelfX_ = 0;
  currentShelfY_ = 0;
  currentShelfH_ = 0;
  dirty_ = true;
}

bool GlyphAtlas::packGlyph(int w, int h, int &outX, int &outY) {
  const int paddedW = w + kPadding;
  const int paddedH = h + kPadding;

  // 現在の shelf に収まるか
  if (currentShelfX_ + paddedW <= kAtlasSize) {
    outX = currentShelfX_;
    outY = currentShelfY_;
    currentShelfX_ += paddedW;
    currentShelfH_ = std::max(currentShelfH_, paddedH);
    return true;
  }

  // 次の shelf へ
  const int nextY = currentShelfY_ + currentShelfH_;
  if (nextY + paddedH <= kAtlasSize) {
    currentShelfX_ = 0;
    currentShelfY_ = nextY;
    currentShelfH_ = paddedH;
    outX = currentShelfX_;
    outY = currentShelfY_;
    currentShelfX_ += paddedW;
    return true;
  }

  // atlas が満杯
  return false;
}

GlyphRect GlyphAtlas::acquire(const GlyphKey &key, const QFont &font) {
  // キャッシュヒット
  auto it = cache_.find(key);
  if (it != cache_.end()) {
    return it->second;
  }

  // QRawFont でグリフをラスタライズ
  const QString sample = QString::fromUcs4(&key.codePoint, 1);
  QRawFont rawFont = QRawFont::fromFont(font, QFontDatabase::Any);

  const QVector<quint32> indices = rawFont.glyphIndexesForString(sample);
  if (indices.isEmpty()) {
    GlyphRect empty;
    empty.valid = false;
    cache_[key] = empty;
    return empty;
  }

  const quint32 gindex = indices.first();
  QImage glyphBitmap =
      rawFont.alphaMapForGlyph(gindex, QRawFont::PixelAntialiasing);

  // アルファマップが空またはサポートされないコードポイント
  if (glyphBitmap.isNull() || glyphBitmap.width() <= 0 ||
      glyphBitmap.height() <= 0) {
    GlyphRect empty;
    empty.valid = false;
    cache_[key] = empty;
    return empty;
  }

  const int gw = glyphBitmap.width();
  const int gh = glyphBitmap.height();

  // alpha map を RGBA8 に展開する。
  // shader 側は alpha をサンプリングするので RGB は白で良い。
  QImage glyphRgba(gw, gh, QImage::Format_RGBA8888);
  glyphRgba.fill(Qt::transparent);
  for (int y = 0; y < gh; ++y) {
    auto *dst = reinterpret_cast<QRgb *>(glyphRgba.scanLine(y));
    for (int x = 0; x < gw; ++x) {
      const int alpha = glyphBitmap.pixelColor(x, y).alpha();
      dst[x] = qRgba(255, 255, 255, alpha);
    }
  }

  // atlas が満杯の場合はリセット
  int px = 0, py = 0;
  if (!packGlyph(gw, gh, px, py)) {
    clear();
    if (!packGlyph(gw, gh, px, py)) {
      // グリフ単体でも収まらないサイズ（異常）
      GlyphRect empty;
      empty.valid = false;
      cache_[key] = empty;
      return empty;
    }
  }

  // atlas に書き込む
  {
    QPainter p(&atlasImage_);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.drawImage(px, py, glyphRgba);
  }

  // メトリクスを収集
  QFontMetricsF fm(font);
  const QRectF br = rawFont.boundingRect(gindex);

  GlyphRect rect;
  rect.atlasX = px;
  rect.atlasY = py;
  rect.width = gw;
  rect.height = gh;
  rect.bearingX = static_cast<float>(br.left());
  rect.bearingY = static_cast<float>(-br.top()); // Qt: top は負
  const QVector<quint32> glyphIndexes{gindex};
  const QVector<QPointF> advances = rawFont.advancesForGlyphIndexes(glyphIndexes);
  rect.advance = static_cast<float>(advances.isEmpty() ? 0.0 : advances.first().x());
  rect.valid = true;

  dirty_ = true;
  cache_[key] = rect;
  return rect;
}

} // namespace ArtifactCore
