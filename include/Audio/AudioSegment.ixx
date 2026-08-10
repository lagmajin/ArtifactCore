module;
#include <utility>
#include <algorithm>
#include <limits>
#include <QtGlobal>
#include <QVector>

export module Audio.Segment;

export namespace ArtifactCore {
 enum class AudioChannelLayout {
  Mono,       // 1ch
  Stereo,     // 2ch
  Surround51, // 6ch (L, R, C, LFE, Ls, Rs)
  Surround71, // 8ch
  Custom10ch, // 10ch
  Ambisonics  // 球面調和関数（VRなどで利用）
 };

 enum class AudioChannelType {
  Left, Right, Center, LFE, LeftSurround, RightSurround,
  LeftBack, RightBack, TopFrontLeft, TopFrontRight, // 10ch用など
  Unknown
 };

 struct AudioSegment {
  // 各チャンネルのサンプルデータを格納する二次元構造
  // channels[0] = Left, channels[1] = Right ...
  // QVectorの暗黙的共有により、この構造体自体のコピーコストは非常に低い
  QVector<QVector<float>> channelData;

  int sampleRate = 44100;
  AudioChannelLayout layout = AudioChannelLayout::Stereo;
  qint64 startFrame = 0;

  // 便利関数：全チャンネルのサンプル数を取得
  int frameCount() const {
   if (channelData.isEmpty()) return 0;
   const auto maxInt = static_cast<qsizetype>(std::numeric_limits<int>::max());
   int frames = static_cast<int>(std::min(channelData[0].size(), maxInt));
   for (qsizetype channel = 1; channel < channelData.size(); ++channel) {
    const int channelFrames = static_cast<int>(
        std::min(channelData[channel].size(), maxInt));
    frames = std::min(frames, channelFrames);
   }
   return frames;
  }

  void setFrameCount(int frames) {
    const int safeFrames = std::max(0, frames);
    for (auto& ch : channelData) ch.resize(safeFrames);
  }

  void clear() {
    channelData.clear();
  }

  void zero() {
    for (auto& ch : channelData) ch.fill(0.0f);
  }

  // チャンネル数を取得
  int channelCount() const {
   return static_cast<int>(std::min(
       channelData.size(), static_cast<qsizetype>(std::numeric_limits<int>::max())));
  }

  // 特定のサンプルのポインタを安全に取得
  const float* constData(int channelIdx) const {
   if (channelIdx >= 0 && static_cast<qsizetype>(channelIdx) < channelData.size()) {
	return channelData[channelIdx].constData();
   }
   return nullptr;
  }
 };
};
