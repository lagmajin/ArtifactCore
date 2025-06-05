module;
#include<type_traits>
#include <opencv2/core/types.hpp>
#include <QPointF>
export module Point2D;




export namespace ArtifactCore {

 template<typename T>
 concept Point2DF = requires(T p) {
  { getX(p) } -> std::convertible_to<float>;
  { getY(p) } -> std::convertible_to<float>;
 };


 inline float getX(const cv::Point2f& p) { return p.x; }
 inline float getY(const cv::Point2f& p) { return p.y; }


 inline float getX(const QPointF& p) { return static_cast<float>(p.x()); }
 inline float getY(const QPointF& p) { return static_cast<float>(p.y()); }
}