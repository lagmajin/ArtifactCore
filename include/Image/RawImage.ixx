module;
#include <QVector>
#include <QString>

export module Image.Raw;

export namespace ArtifactCore {

 struct RawImage {
  int width = 0;
  int height = 0;
  int channels = 0;
  QString pixelType; // std::string ���� QString �֕ύX

  // ���̃s�N�Z���f�[�^�Bstd::vector<uint8_t> ���� QVector<quint8> �֕ύX
  QVector<quint8> data; // quint8 �� Qt �� unsigned char

  RawImage() = default;
  ~RawImage() = default;
  RawImage(const RawImage&) = default;
  RawImage& operator=(const RawImage&) = default;
  RawImage(RawImage&&) noexcept = default;
  RawImage& operator=(RawImage&&) noexcept = default;

  bool isValid() const;

  // getPixelTypeSizeInBytes() �̖߂�l�� size_t ���� int �ɕύX (Qt �̊���ɍ��킹�邽��)
  int getPixelTypeSizeInBytes() const;
 };


};
