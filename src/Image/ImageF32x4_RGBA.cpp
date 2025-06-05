module;
#include <opencv2/core.hpp>
module ImageF32x4_RGBA;




namespace ArtifactCore {

 struct ImageF32x4_RGBA::Impl {
  cv::Mat mat;
  Impl() : mat(cv::Size(0, 0), CV_32FC4) {}
 };

 ImageF32x4_RGBA::ImageF32x4_RGBA()
  : impl_(std::make_unique<Impl>()) {
 }

 ImageF32x4_RGBA::ImageF32x4_RGBA(const FloatRGBA& color)
 {

 }

 void ImageF32x4_RGBA::fill(const FloatRGBA& rgba)
 {
 
   // OpenCV��BGR(A)�̏����g�����A�����ł�FloatRGBA��RGBA�Ɖ���
   cv::Vec4f color(rgba.r(), rgba.g(), rgba.b(), rgba.a()); // �v�C���F���Ԏ���ł́ir, g, b, a�j���ib, g, r, a�j
   impl_->mat.setTo(color);
  
 }

 void ImageF32x4_RGBA::resize(int width, int height)
 {

 }


  ImageF32x4_RGBA ImageF32x4_RGBA::createMaskLike(const ImageF32x4_RGBA& src, const FloatRGBA& fillColor /*= FloatRGBA*/)
  {
   ImageF32x4_RGBA mask;
   mask = ImageF32x4_RGBA(); // �f�t�H���g�R���X�g���N�^�ŏ�����
   mask.resize(src.width(), src.height()); // impl��resize���\�b�h������Ύg���z��
   mask.fill(fillColor);
   return mask;
  }

  ImageF32x4_RGBA& ImageF32x4_RGBA::operator=(const ImageF32x4_RGBA& other)
  {
   if (this != &other) {
	if (other.impl_) {
	 impl_ = std::make_unique<Impl>(*other.impl_);  // �� Impl �ɃR�s�[�R���X�g���N�^���K�v�I
	}
	else {
	 impl_.reset();  // nullptr ����
	}
   }
   return *this;
  }

 ImageF32x4_RGBA::~ImageF32x4_RGBA() = default;




};