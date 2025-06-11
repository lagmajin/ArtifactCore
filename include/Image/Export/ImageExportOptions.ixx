module;

#include <QString>
#include <OpenImageIO/imageio.h> // OIIO
#include <OpenImageIO/typedesc.h> // O
export module Image:ImageExports;




export namespace ArtifactCore {

 struct ImageExportOptions {
  // �t�@�C���t�H�[�}�b�g (OIIO������)
  QString format;
  // ���k���� (OIIO������)
  QString compression;
  // ���k�i�� (0.0f - 100.0f)
  float compressionQuality;
  // �f�[�^�^ (OIIO TypeDesc)
  //TypeDesc dataType;
  // �F��� (OIIO������)
  QString colorSpace;
  // �^�C����
  int tileWidth;
  // �^�C������
  int tileHeight;
  // �쐬�҃��^�f�[�^
  QString creator;
  // �R�s�[���C�g���^�f�[�^
  QString copyright;

  // �f�t�H���g�l
  ImageExportOptions() :
   format("exr"),
   compression("zip"),
   compressionQuality(90.0f),
   //dataType(TypeDesc::HALF), // �f�t�H���g�̓v���_�N�V�����ň�ʓI��Half
   colorSpace("acescg"),
   tileWidth(0), // 0�̓^�C�������Ȃ�
   tileHeight(0),
   creator(""),
   copyright("")
  {
  }
 };

}