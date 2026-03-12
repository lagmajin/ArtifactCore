module;
#include <QRectF>
#include <QSizeF>
#include <vector>
export module CalcSafeArea;

export namespace ArtifactCore {

 enum class SafeFrameType {
  Action, // Standard action-safe 90%
  Title,  // Standard title-safe 80%
  Custom  // Arbitrary safe area, e.g. for social UI
 };

 struct SafeRect {
  SafeFrameType type;
  QRectF rect;
  float ratio;
 };

};
