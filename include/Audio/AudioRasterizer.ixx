module;
#include <QVector>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QColor>

export module Audio.Rasterizer;

import Audio.Segment;





export namespace ArtifactCore
{
 struct WaveformData {
  QVector<float> minValues;
  QVector<float> maxValues;
 };

 class AudioRasterizer
 {
 private:
  class Impl;
  Impl* impl_ = nullptr;

 public:
  AudioRasterizer();
  ~AudioRasterizer();

  // 設定
  void setDisplaySize(int width, int height);
  void setUseRMS(bool use);

  // ラスタライズ
  WaveformData rasterize(const AudioSegment& segment);
  WaveformData rasterizeRange(const AudioSegment& segment, int startSample, int sampleCount);

  // 画像レンダリング
  QImage renderToImage(const WaveformData& data);
 };

};
