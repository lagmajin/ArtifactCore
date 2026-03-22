module;
#include <libavutil/samplefmt.h>
#include <QByteArray>
#include <iostream>
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
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Audio.Frame;






export namespace ArtifactCore {

 struct AudioFrame {
  QByteArray pcmData;       // PCMデータ（16bit, 32bitなど）
  int sampleRate = 0;       // サンプリングレート（例：44100Hz）
  int channels = 0;         // チャンネル数（例：2）
  AVSampleFormat format = AV_SAMPLE_FMT_NONE; // FFmpeg形式の保持も可能（変換に便利）
  qint64 pts = 0;           // presentation timestamp（ミリ秒 or AVStreamのtime_base基準）

  bool isValid() const {
   return !pcmData.isEmpty() && sampleRate > 0 && channels > 0;
  }
 };





};