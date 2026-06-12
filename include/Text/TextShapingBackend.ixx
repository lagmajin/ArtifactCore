module;
#include <QVector>
#include <QString>

export module Text.ShapingBackend;

import std;
import Text.LayoutContract;
import Text.Style;

namespace ArtifactCore {

export struct TextShapingRequest {
  QString text;
  TextStyle style;
  ParagraphStyle paragraph;
  TextWritingMode writingMode = TextWritingMode::Horizontal;
  TextDirection baseDirection = TextDirection::Auto;
  QString locale;
  QVector<TextRubyAttachment> rubyAttachments;
};

export struct TextShapingResult {
  std::vector<GlyphItem> glyphs;
  QVector<int> logicalToVisual;
  QVector<int> visualToLogical;
  TextLayoutContract contract;
};

export class ITextShapingBackend {
public:
  virtual ~ITextShapingBackend() = default;
  virtual TextShapingResult shape(const TextShapingRequest& request) = 0;
};

export class QtShapingBackend final : public ITextShapingBackend {
public:
  TextShapingResult shape(const TextShapingRequest& request) override;
};

export class HarfBuzzShapingBackend final : public ITextShapingBackend {
public:
  TextShapingResult shape(const TextShapingRequest& request) override;
};

} // namespace ArtifactCore
