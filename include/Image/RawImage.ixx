module;
#include <QVector>
#include <QString>

export module Image.Raw;

export namespace ArtifactCore {

 struct RawImage {
  int width = 0;
  int height = 0;
  int channels = 0;
  QString pixelType; // std::string から QString へ変更

  // 生のピクセルデータ。std::vector<uint8_t> から QVector<quint8> へ変更
  QVector<quint8> data; // quint8 は Qt の unsigned char

  RawImage() = default;
  ~RawImage() = default;
  RawImage(const RawImage&) = default;
  RawImage& operator=(const RawImage&) = default;
  RawImage(RawImage&&) noexcept = default;
  RawImage& operator=(RawImage&&) noexcept = default;

  bool isValid() const;

  // getPixelTypeSizeInBytes() の戻り値を size_t から int に変更 (Qt の慣例に合わせるため)
  int getPixelTypeSizeInBytes() const;
 };


};
