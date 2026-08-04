module;
#include <utility>
#include <QString>
#include <vector>


export module Image:OpenEXR;

import Image.DeepImageBuffer;

namespace ArtifactCore {

 struct OpenExrDeepSample {
  float depth = 0.0f;
  float red = 0.0f;
  float green = 0.0f;
  float blue = 0.0f;
  float alpha = 0.0f;
 };

 class OpenExrPrivate;

 class OpenExr {
 private:

 public:
  OpenExr();
  ~OpenExr();
  // Low-level float RGBA writer for callers that already own linear pixels.
  bool writeRGBA32F(const QString& path, const float* rgba, int width, int height,
                    const QString& compression = QStringLiteral("zip"));
  bool readRGBA32F(const QString& path, std::vector<float>& rgba,
                   int& width, int& height) const;
  // Writes one or more depth-ordered RGBA samples for every pixel.
  // samples must contain width * height entries; an entry may be empty.
  bool writeDeepRGBA32F(
      const QString& path, int width, int height,
      const std::vector<std::vector<OpenExrDeepSample>>& samples,
      const QString& compression = QStringLiteral("zip"));
  bool writeDeepRGBA32F(
      const QString& path, const DeepImageBuffer& image,
      const QString& compression = QStringLiteral("zip"));
  bool readDeepRGBA32F(
      const QString& path, std::vector<std::vector<OpenExrDeepSample>>& samples,
      int& width, int& height) const;
  bool readDeepRGBA32F(
      const QString& path, DeepImageBuffer& image) const;
  // Merges two per-pixel deep layers while preserving front-to-back depth order.
  // Inputs must use the same pixel count and nondecreasing depth order.
  static bool mergeDeepRGBA32F(
      const std::vector<std::vector<OpenExrDeepSample>>& front,
      const std::vector<std::vector<OpenExrDeepSample>>& back,
      std::vector<std::vector<OpenExrDeepSample>>& merged);
  // Flattens front-to-back premultiplied deep samples into RGBA32F pixels.
  static bool flattenDeepRGBA32F(
      const std::vector<std::vector<OpenExrDeepSample>>& samples,
      int width, int height, std::vector<float>& rgba);
 };





};
