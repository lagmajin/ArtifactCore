
module;
#include <utility>
#include <Halide.h>
#include "../../../include/Define/DllExportMacro.hpp"
#include <opencv2/opencv.hpp>
module ImageProcessing:Halide;





namespace ArtifactCore {
 using namespace Halide;

 Func makeLuminanceFunc(const Func& inputRGBA) {
  Var x("x"), y("y");

  Expr r = inputRGBA(x, y, 0);
  Expr g = inputRGBA(x, y, 1);
  Expr b = inputRGBA(x, y, 2);
  Expr a = inputRGBA(x, y, 3);

  Expr lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;

  Func luminanceFunc("luminanceFunc");
  luminanceFunc(x, y, 0) = lum;
  luminanceFunc(x, y, 1) = lum;
  luminanceFunc(x, y, 2) = lum;
  luminanceFunc(x, y, 3) = a;

  return luminanceFunc;
 }
 using namespace Halide;
 // Halide �����e�X�g�FOpenCV�摜��Halide�ŕϊ�
 LIBRARY_DLL_API cv::Mat halideTest2(const cv::Mat& rgba32f) {
  assert(rgba32f.type() == CV_32FC4);
  int w = rgba32f.cols;
  int h = rgba32f.rows;

  
  Var x("x"), y("y"), c("c");

  // Planar buffer �ɕϊ� (c,x,y)
  Buffer<float> input(4, w, h);
  for (int y = 0; y < h; ++y) {
   const cv::Vec4f* row = rgba32f.ptr<cv::Vec4f>(y);
   for (int x = 0; x < w; ++x)
	for (int c = 0; c < 4; ++c)
	 input(c, x, y) = row[x][c];
  }

  // Halide Func �ɓn��
  Func inputFunc("inputFunc");
  inputFunc(x, y, c) = input(c, x, y);

  // �P�x�ϊ�
  Func luminanceFunc = makeLuminanceFunc(inputFunc);

  // �o�͂� planar
  Buffer<float> outBuf(4, w, h);
  luminanceFunc.realize(outBuf);

  // OpenCV �ɖ߂�
  cv::Mat output(h, w, CV_32FC4);
  for (int y = 0; y < h; ++y) {
   cv::Vec4f* row = output.ptr<cv::Vec4f>(y);
   for (int x = 0; x < w; ++x)
	for (int c = 0; c < 4; ++c)
	 row[x][c] = outBuf(c, x, y);
  }

  return output;
 }

 LIBRARY_DLL_API cv::Mat halideTestMinimal(const cv::Mat& inputRGBA32F) {
  using namespace Halide;
  assert(inputRGBA32F.type() == CV_32FC4);

  int w = inputRGBA32F.cols;
  int h = inputRGBA32F.rows;

  // 1. cv::Mat -> Halide::Buffer<float> (c, x, y)
  Halide::Buffer<float> buffer(4, w, h);
  for (int y = 0; y < h; ++y) {
   const cv::Vec4f* row = inputRGBA32F.ptr<cv::Vec4f>(y);
   for (int x = 0; x < w; ++x) {
	for (int c = 0; c < 4; ++c) {
	 buffer(c, x, y) = row[x][c];
	}
   }
  }

  // 2. �������Ȃ�Halide�֐��쐬
  Halide::Func identity("identity");
  Halide::Var x, y, c;
  identity(x, y, c) = buffer(c, x, y);

  // 3. �o�̓o�b�t�@�p��
  Halide::Buffer<float> outputBuffer(4, w, h);
  identity.realize(outputBuffer);

  // 4. Halide::Buffer -> cv::Mat
  cv::Mat output(h, w, CV_32FC4);
  for (int y = 0; y < h; ++y) {
   cv::Vec4f* outRow = output.ptr<cv::Vec4f>(y);
   for (int x = 0; x < w; ++x) {
	for (int c = 0; c < 4; ++c) {
	 outRow[x][c] = outputBuffer(c, x, y);
	}
   }
  }

  return output;
 }

 LIBRARY_DLL_API cv::Mat halideTestMinimal2(const cv::Mat& inputRGBA32F)
 {
  // �T�C�Y���ߑł��i�ŏ��e�X�g�p�j
  const int w = 4;
  const int h = 2;

  // Halide ���� RGBA �l�����o�b�t�@�𐶐�
  Halide::Buffer<float> buffer(w, h, 4);
  for (int y = 0; y < h; ++y) {
   for (int x = 0; x < w; ++x) {
	buffer(x, y, 0) = 1.0f; // R
	buffer(x, y, 1) = 0.0f; // G
	buffer(x, y, 2) = 0.0f; // B
	buffer(x, y, 3) = 1.0f; // A
   }
  }

  // OpenCV �� CV_32FC4 �`���ŏo��Mat���쐬
  cv::Mat output(h, w, CV_32FC4);
  for (int y = 0; y < h; ++y) {
   cv::Vec4f* row = output.ptr<cv::Vec4f>(y);
   for (int x = 0; x < w; ++x) {
	for (int c = 0; c < 4; ++c) {
	 row[x][c] = buffer(x, y, c);
	}
   }
  }

  return output;
 }


}
