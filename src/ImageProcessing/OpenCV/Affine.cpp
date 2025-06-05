#include <opencv2/opencv.hpp>


import Transform2D;

namespace ArtifactCore {

 using namespace cv;

 void Affine2DCV(const cv::Mat mat, Transform2D transform2D,cv::Mat dsc)
 {
  cv::Mat rotMat = cv::getRotationMatrix2D(transform2D.anchor(), transform2D.rotation(), 1.0);
 
  rotMat.at<double>(0, 0) *= transform2D.scaleX();
  rotMat.at<double>(0, 1) *= transform2D.scaleX();
  rotMat.at<double>(1, 0) *= transform2D.scaleY();
  rotMat.at<double>(1, 1) *= transform2D.scaleY();

  //���s�ړ��𔽉f�i��]�E�X�P�[����ɍŏI�ʒu�ɍ��킹��j
 rotMat.at<double>(0, 2) += transform2D.x();
  rotMat.at<double>(1, 2) += transform2D.y();

  //�A�t�B���ϊ��̎��s
  cv::warpAffine(mat, dsc, rotMat, mat.size(), cv::INTER_LINEAR, cv::BORDER_REFLECT);
 
 }


}