module;
class tst_QList;
#include <iostream>
#include <algorithm>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QVector>
#include <QImage>
export module Audio.Rasterizer;

export namespace ArtifactCore
{
 struct WaveformData {
  QVector<float> minValues;
  QVector<float> maxValues;
  bool isEmpty() const { return minValues.isEmpty() || maxValues.isEmpty(); }
  int binCount() const { return std::min(minValues.size(), maxValues.size()); }
 };

 class AudioRasterizer
 {
 private:


 public:
  static WaveformData rasterize(const QVector<float>& samples, int binCount);
  static WaveformData rasterizeInterleaved(const QVector<float>& samples,
                                            int channels, int binCount);

 };

};
