
module;
#include <opencv2/core/mat.hpp>
export module Image:ImageTransform;





export namespace ArtifactCore{

 cv::Mat convertToFloat32RGBA(const cv::Mat& input);
 
 // Scale image to new width/height (bilinear)
 cv::Mat scaleImage(const cv::Mat& input, int newWidth, int newHeight);

 // Rotate image by degrees around center (expand=false keeps same size)
 cv::Mat rotateImage(const cv::Mat& input, double degrees, bool expand = false);

 // Apply 2x3 affine transform matrix (warpAffine)
 cv::Mat affineTransform(const cv::Mat& input, const cv::Mat& matrix, cv::Size dsize);




}