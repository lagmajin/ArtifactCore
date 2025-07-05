module;
#include <QDirIterator>
#include <QImage>
#include <QCoreApplication>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>
#include "../../Define/DllExportMacro.hpp"

export module SearchImage;


export namespace ArtifactCore
{
LIBRARY_DLL_API cv::Mat loadImageFromExePath(const QString& image_filename, int target_cv_format, bool recursive = true)
 {
  QString appDirPath = QCoreApplication::applicationDirPath();
  qDebug() << "�A�v���P�[�V�����f�B���N�g��:" << appDirPath;

  // �摜�t�@�C��������
  // QDirIterator ���g�p���āA�w�肳�ꂽ�f�B���N�g���i�ƃT�u�f�B���N�g���j���̃t�@�C����T�����܂��B
  QDirIterator it(appDirPath, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);

  QString foundPath;
  while (it.hasNext()) {
   QString currentPath = it.next();
   QFileInfo fileInfo(currentPath);

   // �t�@�C���ł��邱�ƁA����уt�@�C��������v���邱�Ƃ��m�F�i�啶������������ʂ��Ȃ��j
   if (fileInfo.isFile() && fileInfo.fileName().toLower() == image_filename.toLower()) {
	foundPath = currentPath;
	break; // ���������烋�[�v���I��
   }
  }

  if (foundPath.isEmpty()) {
   qWarning() << "�摜��������܂���ł���:" << image_filename;
   return cv::Mat(); // �摜��������Ȃ������ꍇ�͋��Mat��Ԃ�
  }

  qDebug() << "�摜��������܂���:" << foundPath;

  // QImage�ŉ摜�����[�h
  QImage qImage(foundPath);
  if (qImage.isNull()) {
   qWarning() << "QImage�̃��[�h�Ɏ��s���܂���:" << foundPath;
   return cv::Mat();
  }

  // QImage�̃t�H�[�}�b�g�� cv::Mat �Ɉ����₷�� Format_ARGB32 �ɕϊ����܂��B
  // QImage::Format_ARGB32 �́A�ʏ탊�g���G���f�B�A���V�X�e���� 0xAARRGGBB �̌`���Ŋi�[����A
  // ����̓�������ł̓o�C�g���� B, G, R, A �ƂȂ�܂��B
  // ���̂��߁Acv::Mat �� CV_8UC4 �ɒ��ڃ}�b�v����ƁABGRA ���̉摜�Ƃ��ēK�؂ɉ��߂���܂��B
  if (qImage.format() != QImage::Format_ARGB32) {
   qImage = qImage.convertToFormat(QImage::Format_ARGB32);
   if (qImage.isNull()) {
	qWarning() << "QImage����Format_ARGB32�ւ̕ϊ��Ɏ��s���܂����B";
	return cv::Mat();
   }
  }

  // QImage�̐��f�[�^�|�C���^�ƃX�g���C�h���g���� cv::Mat ���\�z���܂��B
  // QImage::bits() �͉摜�f�[�^�̐擪�|�C���^�AbytesPerLine() ��1�s������̃o�C�g���ł��B
  // CV_8UC4 (�����Ȃ�8�r�b�g�A4�`�����l��) �Œ��� QImage �̃f�[�^�ɃA�N�Z�X���܂��B
  cv::Mat cvMat_8UC4(qImage.height(), qImage.width(), CV_8UC4, (void*)qImage.bits(), qImage.bytesPerLine());

  cv::Mat finalCvMat;

  // �ڕW��OpenCV�t�H�[�}�b�g (CV_32FC4��z��) �ɕϊ����܂��B
  // CV_8UC4 (0-255�͈̔�) ���� CV_32FC4 (0.0-1.0�͈̔�) �ւ̃X�P�[�����O�ƌ^�ϊ��𓯎��ɍs���܂��B
  if (cvMat_8UC4.type() != target_cv_format) {
   cvMat_8UC4.convertTo(finalCvMat, target_cv_format, 1.0 / 255.0); // 0-255 -> 0.0-1.0
  }
  else {
   // ���łɖڕW�t�H�[�}�b�g�̏ꍇ�AQImage�̓����f�[�^�ւ̎Q�Ƃ�����邽�߂ɃR�s�[���쐬���܂��B
   cvMat_8UC4.copyTo(finalCvMat);
  }

  // QImage::Format_ARGB32 ���� CV_8UC4 �ւ͘_���I��BGRA���ɂȂ邽�߁A
  // ���̌�� convertTo(CV_32FC4) ��BGRA�����ێ����܂��B
  // ���������āAcv::cvtColor �ɂ�閾���I�ȃ`���l���X���b�v�͕s�v�ł��B

  return finalCvMat;
 }

LIBRARY_DLL_API cv::Mat findAndLoadImageInAppDir(const QString& image_filename, int target_cv_format)
{
 QString appDirPath = QCoreApplication::applicationDirPath();
 qDebug() << "�A�v���P�[�V�����f�B���N�g��:" << appDirPath;
 qDebug() << "�A�v���P�[�V�����f�B���N�g���ȉ��̂��ׂẴt�@�C������ '" << image_filename << "' ��������...";

 // QDirIterator ���g�p���āA�A�v���P�[�V�����f�B���N�g���Ƃ��̂��ׂẴT�u�f�B���N�g�����̃t�@�C����T�����܂��B
 // QDir::Files �� QDir::NoDotAndDotDot ���w�肵�AQDirIterator::Subdirectories �ōċA������L���ɂ��܂��B
 QDirIterator it(appDirPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);

 QString foundPath;
 while (it.hasNext()) {
  QString currentPath = it.next();
  QFileInfo fileInfo(currentPath);

  // �t�@�C�������w�肳�ꂽ image_filename �ƈ�v���邱�Ƃ��m�F�i�啶������������ʂ��Ȃ��j
  if (fileInfo.fileName().toLower() == image_filename.toLower()) {
   foundPath = currentPath;
   break; // ���������烋�[�v���I��
  }
 }

 if (foundPath.isEmpty()) {
  qWarning() << "target image'" << image_filename << "' �̓A�v���P�[�V�����f�B���N�g���ȉ��Ō�����܂���ł����B";
  return cv::Mat(); // �摜��������Ȃ������ꍇ�͋��Mat��Ԃ�
 }

 qDebug() << "image founded:" << foundPath;

 cv::Mat loadedCvMat = cv::imread(foundPath.toStdString());
 if (loadedCvMat.empty()) {
  qWarning() << "OpenCV�ł̉摜���[�h�Ɏ��s���܂���:" << foundPath;
  return cv::Mat();
 }

 cv::Mat intermediateCvMat;

 // �ǂݍ���Mat�̃`�����l�������m�F���A�K�v�ł����4�`�����l���ɕϊ�
 if (loadedCvMat.channels() == 3) {
  // 3�`�����l�� (BGR) �̏ꍇ�A4�`�����l�� (BGRA) �ɕϊ��i�A���t�@�`�����l����1.0�Ŗ��߂�j
  cv::cvtColor(loadedCvMat, intermediateCvMat, cv::COLOR_BGR2BGRA);
 }
 else if (loadedCvMat.channels() == 4) {
  // ����4�`�����l�� (BGRA) �̏ꍇ
  loadedCvMat.copyTo(intermediateCvMat); // ���f�[�^�𒼐ڎg�킸�R�s�[����������S
 }
 else {
  qWarning() << "�T�|�[�g����Ă��Ȃ��`�����l�����̉摜�ł� (OpenCV):" << loadedCvMat.channels();
  return cv::Mat();
 }

 cv::Mat finalCvMat;

 // �ڕW��OpenCV�t�H�[�}�b�g (CV_32FC4��z��) �ɕϊ����܂��B
 // CV_8UC4 (0-255�͈̔�) ���� CV_32FC4 (0.0-1.0�͈̔�) �ւƃX�P�[�����O�ƌ^�ϊ��𓯎��ɍs���܂��B
 if (intermediateCvMat.type() != target_cv_format) {
  // convertTo��scale�����ɂ��A0-255�̐����l��0.0-1.0�̕��������_���ɐ��K������܂��B
  intermediateCvMat.convertTo(finalCvMat, target_cv_format, 1.0 / 255.0);
 }
 else {
  // ���łɖڕW�t�H�[�}�b�g�̏ꍇ
  intermediateCvMat.copyTo(finalCvMat);
 }

 // OpenCV�� cv::imread �͒ʏ� BGR/BGRA ���œǂݍ��݁A
 // convertTo �����̏������ێ����܂��B
 // Diligent Engine��TEX_FORMAT_RGBA32_FLOAT�͓����I��RGBA�Ƃ��Ĉ����܂����A
 // �V�F�[�_�[��ID.xy�A�N�Z�X���s���ۂɁACPU����BGRA���ƍ����悤�ɒ������܂��B
 // ���̊֐������BGRA����CV_32FC4���Ԃ����̂ŁA�V�F�[�_�[���̉��߂ɒ��ӂ��Ă��������B

 return finalCvMat;
}













};