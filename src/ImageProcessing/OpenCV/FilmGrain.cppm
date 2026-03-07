#include <opencv2/gapi.hpp>


namespace ArtifactCore {

 G_TYPED_KERNEL(GFilter2D,
  <cv::GMat(cv::GMat, int, cv::Mat, cv::Point, double, int, cv::Scalar)>,
  "org.opencv.imgproc.filters.filter2D")
 {
  static cv::GMatDesc                 // outMeta's return value type
   outMeta(cv::GMatDesc    in,  // descriptor of input GMat
	int             ddepth,  // depth parameter
	cv::Mat      /* coeffs */,  // (unused)
	cv::Point    /* anchor */,  // (unused)
	double       /* scale  */,  // (unused)
	int          /* border */,  // (unused)
	cv::Scalar   /* bvalue */) // (unused)
  {
   return in.withDepth(ddepth);
  }
 };

}