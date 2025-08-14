module;
#include <opencv2/opencv.hpp>

module ImageProcessing:AffineTransform;

import Transform._2D;

namespace ArtifactCore {

 using namespace cv;

 void Affine2DCV(const cv::Mat mat, Transform2D transform2D,cv::Mat dsc)
 {
 // cv::Mat rotMat = cv::getRotationMatrix2D(transform2D.anchor(), transform2D.rotation(), 1.0);
 
 // rotMat.at<double>(0, 0) *= transform2D.scaleX();
 // rotMat.at<double>(0, 1) *= transform2D.scaleX();
  //rotMat.at<double>(1, 0) *= transform2D.scaleY();
 // rotMat.at<double>(1, 1) *= transform2D.scaleY();

  //平行移動を反映（回転・スケール後に最終位置に合わせる）
 //rotMat.at<double>(0, 2) += transform2D.x();
 // rotMat.at<double>(1, 2) += transform2D.y();

  //アフィン変換の実行
  //cv::warpAffine(mat, dsc, rotMat, mat.size(), cv::INTER_LINEAR, cv::BORDER_REFLECT);
 
 }


}