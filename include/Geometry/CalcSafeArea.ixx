module;
#include <QRectF>
#include <QSizeF>
#include <vector>
export module CalcSafeArea;

export namespace ArtifactCore {

 enum class SafeFrameType {
  Action, // 一般的に 90%
  Title,  // 一般的に 80%
  Custom  // 任意の割合（SNS用 UI回避など）
 };

 struct SafeRect {
  SafeFrameType type;
  QRectF rect;
  float ratio;
 };


};