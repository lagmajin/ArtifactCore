module;
#include <QFont>
#include <QFontDatabase>
#include <QColor>
#include <QImage>
#include <QRawFont>
#include <QString>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwrite_3.h>
#include <wrl/client.h>
#pragma comment(lib, "dwrite.lib")
#endif


module Text.GlyphAtlas;

import Text.Style;
import Font.FreeFont;

namespace ArtifactCore {

#if defined(_WIN32)
namespace {

using Microsoft::WRL::ComPtr;

bool rasterizeColorGlyphWithDirectWrite(const GlyphKey& key,
                                        const QFont& font,
                                        QImage& output) {
  ComPtr<IDWriteFactory> factory;
  if (FAILED(DWriteCreateFactory(
          DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
          reinterpret_cast<IUnknown**>(factory.GetAddressOf())))) {
    return false;
  }
  ComPtr<IDWriteFactory2> factory2;
  ComPtr<IDWriteFactory3> factory3;
  factory.As(&factory2);
  factory.As(&factory3);
  if (!factory2 || !factory3) return false;

  ComPtr<IDWriteFontCollection> collection;
  if (FAILED(factory->GetSystemFontCollection(&collection))) return false;
  UINT32 familyIndex = 0;
  BOOL exists = FALSE;
  const QString familyName = QString::fromUtf8(key.fontFamily.c_str());
  if (FAILED(collection->FindFamilyName(reinterpret_cast<const WCHAR*>(familyName.utf16()),
                                        &familyIndex, &exists)) || !exists) {
    return false;
  }
  ComPtr<IDWriteFontFamily> family;
  ComPtr<IDWriteFont> dwriteFont;
  ComPtr<IDWriteFontFace> face;
  if (FAILED(collection->GetFontFamily(familyIndex, &family)) ||
      FAILED(family->GetFirstMatchingFont(
          (key.styleFlags & 2u) ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
          DWRITE_FONT_STRETCH_NORMAL,
          (key.styleFlags & 1u) ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
          &dwriteFont)) ||
      FAILED(dwriteFont->CreateFontFace(&face))) {
    return false;
  }

  const QString sequence = key.sequenceUtf8.empty()
                               ? QString::fromUcs4(&key.codePoint, 1)
                               : QString::fromUtf8(key.sequenceUtf8.c_str());
  const QVector<uint> codePoints = sequence.toUcs4();
  if (codePoints.isEmpty() && key.shapedGlyphIndex == 0) return false;
  std::vector<UINT16> glyphIndices;
  if (!key.shapedGlyphIndices.empty()) {
    glyphIndices.reserve(key.shapedGlyphIndices.size());
    for (const auto glyph : key.shapedGlyphIndices) {
      glyphIndices.push_back(static_cast<UINT16>(glyph));
    }
  } else if (key.shapedGlyphIndex != 0) {
    glyphIndices.push_back(static_cast<UINT16>(key.shapedGlyphIndex));
  } else {
    glyphIndices.resize(codePoints.size(), 0);
    if (FAILED(face->GetGlyphIndices(
            reinterpret_cast<const UINT32*>(codePoints.constData()),
            static_cast<UINT32>(codePoints.size()), glyphIndices.data()))) {
      return false;
    }
  }
  DWRITE_FONT_METRICS metrics{};
  face->GetMetrics(&metrics);
  const float scale = metrics.designUnitsPerEm > 0
                          ? key.fontSize / static_cast<float>(metrics.designUnitsPerEm)
                          : 1.0f;
  std::vector<float> advances(glyphIndices.size(), 0.0f);
  std::vector<DWRITE_GLYPH_METRICS> glyphMetrics(glyphIndices.size());
  if (FAILED(face->GetDesignGlyphMetrics(glyphIndices.data(),
                                         static_cast<UINT32>(glyphIndices.size()),
                                         glyphMetrics.data(), FALSE))) {
    return false;
  }
  for (size_t i = 0; i < glyphMetrics.size(); ++i) {
    advances[i] = glyphMetrics[i].advanceWidth * scale;
  }

  DWRITE_GLYPH_RUN baseRun{};
  baseRun.fontFace = face.Get();
  baseRun.fontEmSize = key.fontSize;
  baseRun.glyphCount = static_cast<UINT32>(glyphIndices.size());
  baseRun.glyphIndices = glyphIndices.data();
  baseRun.glyphAdvances = advances.data();
  const bool regionalIndicator = key.codePoint >= 0x1F1E6 &&
                                 key.codePoint <= 0x1F1FF;
  if (regionalIndicator && key.sequenceUtf8.empty()) {
    ComPtr<IDWriteGlyphRunAnalysis> analysis;
    if (FAILED(factory3->CreateGlyphRunAnalysis(
            &baseRun, nullptr, DWRITE_RENDERING_MODE1_NATURAL,
            DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DISABLED,
            DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, 0.0f, 0.0f, &analysis))) {
      return false;
    }
    RECT bounds{};
    if (FAILED(analysis->GetAlphaTextureBounds(DWRITE_TEXTURE_ALIASED_1x1,
                                               &bounds))) return false;
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    if (width <= 0 || height <= 0) return false;
    std::vector<BYTE> alpha(static_cast<size_t>(width) * height);
    if (FAILED(analysis->CreateAlphaTexture(
            DWRITE_TEXTURE_ALIASED_1x1, &bounds, alpha.data(),
            static_cast<UINT32>(alpha.size())))) return false;
    output = QImage(width, height, QImage::Format_RGBA8888);
    output.fill(Qt::transparent);
    for (int y = 0; y < height; ++y) {
      auto* dst = output.scanLine(y);
      for (int x = 0; x < width; ++x) {
        const BYTE a = alpha[static_cast<size_t>(y) * width + x];
        dst[x * 4 + 0] = 255;
        dst[x * 4 + 1] = 255;
        dst[x * 4 + 2] = 255;
        dst[x * 4 + 3] = a;
      }
    }
    return true;
  }
  ComPtr<IDWriteColorGlyphRunEnumerator> runs;
  if (FAILED(factory2->TranslateColorGlyphRun(
          0.0f, key.fontSize, &baseRun, nullptr, DWRITE_MEASURING_MODE_NATURAL,
          nullptr, 0, &runs)) || !runs) {
    return false;
  }

  struct Layer { RECT bounds{}; std::vector<BYTE> alpha; DWRITE_COLOR_F color{}; };
  std::vector<Layer> layers;
  BOOL hasRun = FALSE;
  while (SUCCEEDED(runs->MoveNext(&hasRun)) && hasRun) {
    const DWRITE_COLOR_GLYPH_RUN* run = nullptr;
    if (FAILED(runs->GetCurrentRun(&run)) || !run) return false;
    ComPtr<IDWriteGlyphRunAnalysis> analysis;
    if (FAILED(factory3->CreateGlyphRunAnalysis(
            &run->glyphRun, nullptr, DWRITE_RENDERING_MODE1_NATURAL,
            DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DISABLED,
            DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, run->baselineOriginX,
            run->baselineOriginY, &analysis))) {
      continue;
    }
    RECT bounds{};
    if (FAILED(analysis->GetAlphaTextureBounds(DWRITE_TEXTURE_ALIASED_1x1,
                                               &bounds))) continue;
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    if (width <= 0 || height <= 0) continue;
    Layer layer;
    layer.bounds = bounds;
    layer.color = run->runColor;
    layer.alpha.resize(static_cast<size_t>(width) * height);
    if (FAILED(analysis->CreateAlphaTexture(
            DWRITE_TEXTURE_ALIASED_1x1, &bounds, layer.alpha.data(),
            static_cast<UINT32>(layer.alpha.size())))) continue;
    layers.push_back(std::move(layer));
  }
  if (layers.empty()) return false;

  int minX = layers.front().bounds.left;
  int minY = layers.front().bounds.top;
  int maxX = layers.front().bounds.right;
  int maxY = layers.front().bounds.bottom;
  for (const auto& layer : layers) {
    minX = std::min(minX, static_cast<int>(layer.bounds.left));
    minY = std::min(minY, static_cast<int>(layer.bounds.top));
    maxX = std::max(maxX, static_cast<int>(layer.bounds.right));
    maxY = std::max(maxY, static_cast<int>(layer.bounds.bottom));
  }
  output = QImage(maxX - minX, maxY - minY, QImage::Format_RGBA8888);
  output.fill(Qt::transparent);
  for (const auto& layer : layers) {
    const int width = layer.bounds.right - layer.bounds.left;
    const int height = layer.bounds.bottom - layer.bounds.top;
    for (int y = 0; y < height; ++y) {
      auto* dst = output.scanLine(layer.bounds.top - minY + y) +
                  (layer.bounds.left - minX) * 4;
      for (int x = 0; x < width; ++x) {
        const float sourceA = (layer.alpha[static_cast<size_t>(y) * width + x] / 255.0f) * layer.color.a;
        const float oneMinusA = 1.0f - sourceA;
        dst[x * 4 + 0] = static_cast<uchar>((layer.color.r * sourceA + (dst[x * 4 + 0] / 255.0f) * oneMinusA) * 255.0f);
        dst[x * 4 + 1] = static_cast<uchar>((layer.color.g * sourceA + (dst[x * 4 + 1] / 255.0f) * oneMinusA) * 255.0f);
        dst[x * 4 + 2] = static_cast<uchar>((layer.color.b * sourceA + (dst[x * 4 + 2] / 255.0f) * oneMinusA) * 255.0f);
        dst[x * 4 + 3] = static_cast<uchar>((sourceA + (dst[x * 4 + 3] / 255.0f) * oneMinusA) * 255.0f);
      }
    }
  }
  return true;
}

} // namespace
#endif

// ---------------------------------------------------------------------------
// GlyphAtlas
// ---------------------------------------------------------------------------

GlyphAtlas::GlyphAtlas()
    : atlasImage_(kAtlasSize, kAtlasSize, QImage::Format_RGBA8888) {
  atlasImage_.fill(Qt::transparent);
  dirty_ = true;
}

GlyphAtlas::~GlyphAtlas() = default;

QString GlyphAtlas::debugState() const {
  return QStringLiteral("entries=%1 dirty=%2 shelf=%3,%4 shelfH=%5 size=%6x%7")
      .arg(static_cast<qulonglong>(cache_.size()))
      .arg(dirty_ ? QStringLiteral("true") : QStringLiteral("false"))
      .arg(currentShelfX_)
      .arg(currentShelfY_)
      .arg(currentShelfH_)
      .arg(kAtlasSize)
      .arg(kAtlasSize);
}

void GlyphAtlas::clear() {
  cache_.clear();
  atlasImage_.fill(Qt::transparent);
  currentShelfX_ = 0;
  currentShelfY_ = 0;
  currentShelfH_ = 0;
  dirty_ = true;
}

bool GlyphAtlas::packGlyph(int w, int h, int &outX, int &outY) {
  if (w <= 0 || h <= 0 || w > kAtlasSize || h > kAtlasSize) {
    return false;
  }
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

  QImage glyphBitmap;
  bool colorPreserved = false;
#if defined(_WIN32)
  if (key.renderMode == GlyphRenderMode::ColorBitmap &&
      rasterizeColorGlyphWithDirectWrite(key, font, glyphBitmap)) {
    colorPreserved = true;
  }
#endif

  // QRawFont でグリフをラスタライズ（カラー経路のフォールバックを含む）
  const QString sample = QString::fromUcs4(&key.codePoint, 1);
  QRawFont rawFont = QRawFont::fromFont(font, QFontDatabase::Any);

  const QVector<quint32> indices = rawFont.glyphIndexesForString(sample);
  if (indices.isEmpty()) {
    GlyphRect empty;
    empty.valid = false;
    cache_[key] = empty;
    return empty;
  }

  const quint32 gindex = key.renderMode == GlyphRenderMode::ColorBitmap &&
                                 key.shapedGlyphIndex != 0
                             ? static_cast<quint32>(key.shapedGlyphIndex)
                             : indices.first();
  // Glyph index 0 is the font's .notdef/missing-glyph slot.  It must not be
  // uploaded as a normal glyph: its bitmap is commonly a full replacement
  // rectangle and would make unsupported text look like a valid render.
  if (gindex == 0) {
    GlyphRect empty;
    empty.valid = false;
    cache_[key] = empty;
    return empty;
  }
  // QRawFont's alphaMap is a coverage mask.  It is valid for monochrome
  // fallback, but must not be reported as a color-preserving atlas entry.
  if (!colorPreserved) {
    glyphBitmap = rawFont.alphaMapForGlyph(gindex, QRawFont::PixelAntialiasing);
  }

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
  if (colorPreserved) {
    glyphBitmap = glyphBitmap.convertToFormat(QImage::Format_RGBA8888);
  }
  // alpha map を明示的な8-bit coverageへ正規化してからRGBA8へ展開する。
  // QRawFontの返却形式はバックエンドやQt版で Alpha8/Grayscale8 などが
  //変わり得るため、pixelColor()の暗黙変換には依存しない。
  const bool alpha8 = !colorPreserved &&
                      glyphBitmap.format() == QImage::Format_Alpha8;
  const QImage coverage = !colorPreserved && !alpha8
                              ? glyphBitmap.convertToFormat(QImage::Format_Grayscale8)
                              : QImage();

  // alpha map を RGBA8 に展開する。
  // shader 側は alpha をサンプリングするので RGB は白で良い。
  QImage glyphRgba = colorPreserved
                         ? glyphBitmap
                         : QImage(gw, gh, QImage::Format_RGBA8888);
  if (!colorPreserved) {
    glyphRgba.fill(Qt::transparent);
    for (int y = 0; y < gh; ++y) {
      auto *dst = glyphRgba.scanLine(y);
      const auto *src = alpha8 ? glyphBitmap.constScanLine(y)
                               : coverage.constScanLine(y);
      for (int x = 0; x < gw; ++x) {
        dst[x * 4 + 0] = 255;
        dst[x * 4 + 1] = 255;
        dst[x * 4 + 2] = 255;
        dst[x * 4 + 3] = src[x];
      }
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

  // atlas に直接コピーする。glyph asset生成境界でQtのcomposition pathを
  // 通さず、RGBA8の所有バッファへ明示的に書き込む。
  for (int y = 0; y < gh; ++y) {
    std::memcpy(atlasImage_.scanLine(py + y) + px * 4,
                glyphRgba.constScanLine(y), static_cast<size_t>(gw) * 4u);
  }

  // メトリクスを収集
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
  if (key.shapedGlyphIndices.size() > 1 && colorPreserved) {
    // DirectWrite's color-run rasterizer returns the complete cluster bitmap;
    // scalar QRawFont metrics are not valid for that composite rectangle.
    rect.bearingX = 0.0f;
    rect.bearingY = static_cast<float>(gh);
    rect.advance = static_cast<float>(gw);
  }
  rect.valid = true;
  rect.renderMode = key.renderMode;
  rect.colorPreserved = colorPreserved;

  dirty_ = true;
  cache_[key] = rect;
  return rect;
}

} // namespace ArtifactCore
