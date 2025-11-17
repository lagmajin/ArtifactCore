module;

#include <concepts>
#include <QSize>
#include <QSizeF>

#include <opencv2/opencv.hpp>
export module Utils.Size.Like;


namespace ArtifactCore {

 inline QSize toQSize(const QSize& s) { return s; }
 inline QSize toQSize(const QSizeF& s) { return s.toSize(); }
 inline QSize toQSize(const std::pair<int, int>& s) { return QSize(s.first, s.second); }

 // concept定義
 template <typename T>
 concept SizeLike = requires(T a) {
  { toQSize(a) } -> std::convertible_to<QSize>;
 };



};