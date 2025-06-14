module;
#include <opencv2/opencv.hpp>


module ImageProcessing:Sepia;





namespace ArtifactCore {


 cv::Mat applySepiaF32_4(const cv::Mat& src) {
  CV_Assert(src.type() == CV_32FC4);

  cv::Mat dst(src.size(), CV_32FC4);

  const float sepia[3][3] = {
	  {0.272f, 0.534f, 0.131f},
	  {0.349f, 0.686f, 0.168f},
	  {0.393f, 0.769f, 0.189f}
  };

  for (int y = 0; y < src.rows; ++y) {
   const cv::Vec4f* src_row = src.ptr<cv::Vec4f>(y);
   cv::Vec4f* dst_row = dst.ptr<cv::Vec4f>(y);

   for (int x = 0; x < src.cols; ++x) {
	const cv::Vec4f& px = src_row[x];

	float r = px[2], g = px[1], b = px[0]; // RGB‡
	float r2 = sepia[2][0] * r + sepia[2][1] * g + sepia[2][2] * b;
	float g2 = sepia[1][0] * r + sepia[1][1] * g + sepia[1][2] * b;
	float b2 = sepia[0][0] * r + sepia[0][1] * g + sepia[0][2] * b;

	dst_row[x] = cv::Vec4f(
	 std::min(b2, 1.0f),
	 std::min(g2, 1.0f),
	 std::min(r2, 1.0f),
	 px[3] // alpha‚Í‚»‚Ì‚Ü‚Ü
	);
   }
  }

  return dst;
 }


}