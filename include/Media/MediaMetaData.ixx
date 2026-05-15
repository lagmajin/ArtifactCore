module;

#include <QString>
#include <QDateTime>
#include <QSize>
#include <QStringList>
#include <QMap>

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
export module Media.MetaData;

export namespace ArtifactCore {

 // fBA^Cv
 enum class MediaType {
  Unknown,
  Video,
  Audio,
  Image,
  Subtitle,
  Data
 };

 // rfIR[fbN
 struct VideoCodecInfo {
  QString codecName;              // R[fbNiFh264, hevc, vp9j
  QString codecLongName;          // ڍזiFH.264 / AVC / MPEG-4 AVCj
  QString profile;                // vt@CiFHigh, Mainj
  QString level;                  // xiF4.0, 5.1j
  int bitrate = 0;                // rbg[gibpsj
  
  QString pixelFormat;            // sNZtH[}bgiFyuv420pj
  QString colorSpace;             // FԁiFbt709, bt2020j
  QString colorRange;             // F͈́iFtv, pcj
  
  bool hasAlpha = false;          // At@`l̗L
  int bitsPerComponent = 8;       // R|[lg̃rbg
 };

 // I[fBIR[fbN
 struct AudioCodecInfo {
  QString codecName;              // R[fbNiFaac, mp3, opusj
  QString codecLongName;          // ڍז
  int bitrate = 0;                // rbg[gibpsj
  int sampleRate = 0;             // Tv[giHzj
  int channels = 0;               // `l
  QString channelLayout;          // `lCAEgiFstereo, 5.1j
  QString sampleFormat;           // TvtH[}bgiFfltp, s16j
  int bitsPerSample = 0;          // Tṽrbg
 };

 // Xg[
 struct StreamInfo {
  int index = -1;                 // Xg[CfbNX
  MediaType type = MediaType::Unknown;
  QString language;               // iFjpn, engj
  QString title;                  // ^Cg
  bool isDefault = false;         // ftHgXg[
  bool isForced = false;          // \
  
  // rfIXg[ŗL
  QSize resolution;               // 𑜓x
  double frameRate = 0.0;         // t[[g
  double aspectRatio = 0.0;       // AXyNg
  int64_t frameCount = 0;         // t[
  VideoCodecInfo videoCodec;
  
  // I[fBIXg[ŗL
  AudioCodecInfo audioCodec;
  
  // 
  double duration = 0.0;          // b
  int64_t bitrate = 0;            // rbg[gibpsj
  QMap<QString, QString> metadata; // ̑̃^f[^
 };

 // `v^[
 struct ChapterInfo {
  int id = 0;
  double startTime = 0.0;         // b
  double endTime = 0.0;           // b
  QString title;
  QMap<QString, QString> metadata;
 };

 // fBA^f[^
 struct MediaMetaData {
  // ---- t@C ----
  QString filePath;               // t@CpX
  QString fileName;               // t@C
  QString fileExtension;          // gq
  int64_t fileSize = 0;           // t@CTCYioCgj
  
  QString formatName;             // tH[}bgiFmp4, mkv, avij
  QString formatLongName;         // tH[}bgڍז
  
  // ---- {^f[^ ----
  QString title;                  // ^Cg
  QString artist;                 // A[eBXg/
  QString album;                  // Ao
  QString albumArtist;            // AoA[eBXg
  QString composer;               // Ȏ
  QString genre;                  // W
  QString year;                   // N
  QString date;                   // t
  int track = 0;                  // gbNԍ
  int totalTracks = 0;            // gbN
  int disc = 0;                   // fBXNԍ
  int totalDiscs = 0;             // fBXN
  
  QString comment;                // Rg
  QString description;            // 
  QString copyright;              // 쌠
  QString publisher;              // s
  QString encodedBy;              // GR[hl/c[
  
  QStringList keywords;           // L[[h/^O
  
  // ---- Zp ----
  double duration = 0.0;          // f[Vibj
  int64_t bitrate = 0;            // rbg[gibpsj
  QDateTime creationTime;         // 쐬
  QDateTime modificationTime;     // ύX
  
  // ---- Xg[ ----
  std::vector<StreamInfo> streams;
  
  // rfIXg[̃V[gJbg
  bool hasVideo() const {
   return std::any_of(streams.begin(), streams.end(), 
                      [](const StreamInfo& s) { return s.type == MediaType::Video; });
  }
  
  // I[fBIXg[̃V[gJbg
  bool hasAudio() const {
   return std::any_of(streams.begin(), streams.end(), 
                      [](const StreamInfo& s) { return s.type == MediaType::Audio; });
  }
  
  // Xg[̃V[gJbg
  bool hasSubtitles() const {
   return std::any_of(streams.begin(), streams.end(), 
                      [](const StreamInfo& s) { return s.type == MediaType::Subtitle; });
  }
  
  // ŏ̃rfIXg[擾
  StreamInfo* getFirstVideoStream() {
   for (auto& s : streams) {
    if (s.type == MediaType::Video) return &s;
   }
   return nullptr;
  }
  
  const StreamInfo* getFirstVideoStream() const {
   for (const auto& s : streams) {
    if (s.type == MediaType::Video) return &s;
   }
   return nullptr;
  }
  
  // ŏ̃I[fBIXg[擾
  StreamInfo* getFirstAudioStream() {
   for (auto& s : streams) {
    if (s.type == MediaType::Audio) return &s;
   }
   return nullptr;
  }
  
  const StreamInfo* getFirstAudioStream() const {
   for (const auto& s : streams) {
    if (s.type == MediaType::Audio) return &s;
   }
   return nullptr;
  }
  
  // ---- `v^[ ----
  std::vector<ChapterInfo> chapters;
  
  // ---- JX^^f[^ ----
  QMap<QString, QString> customMetadata;
  
  // ---- v ----
  struct Statistics {
   int64_t totalFrames = 0;
   int64_t droppedFrames = 0;
   double averageBitrate = 0.0;
   double maxBitrate = 0.0;
   double minBitrate = 0.0;
  };
  Statistics statistics;
  
  // ---- TlC/Jo[A[g ----
  QString thumbnailPath;          // TlC摜̃pX
  QByteArray coverArt;            // Jo[A[g摜f[^
  QString coverArtMimeType;       // Jo[A[gMIME^Cv
  
  // ---- [eBeB ----
  
  // ^f[^L`FbN
  bool isValid() const {
   return !filePath.isEmpty() && duration > 0.0;
  }
  
  // lԂǂ߂`Ńf[V擾
  QString getFormattedDuration() const {
   int totalSeconds = static_cast<int>(duration);
   int hours = totalSeconds / 3600;
   int minutes = (totalSeconds % 3600) / 60;
   int seconds = totalSeconds % 60;
   
   if (hours > 0) {
    return QString("%1:%2:%3")
           .arg(hours)
           .arg(minutes, 2, 10, QChar('0'))
           .arg(seconds, 2, 10, QChar('0'));
   } else {
    return QString("%1:%2")
           .arg(minutes)
           .arg(seconds, 2, 10, QChar('0'));
   }
  }
  
  // lԂǂ߂`Ńt@CTCY擾
  QString getFormattedFileSize() const {
   double size = static_cast<double>(fileSize);
   
   if (size >= 1024.0 * 1024.0 * 1024.0) {
    return QString::number(size / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
   } else if (size >= 1024.0 * 1024.0) {
    return QString::number(size / (1024.0 * 1024.0), 'f', 2) + " MB";
   } else if (size >= 1024.0) {
    return QString::number(size / 1024.0, 'f', 2) + " KB";
   } else {
    return QString::number(size, 'f', 0) + " B";
   }
  }
  
  // lԂǂ߂`Ńrbg[g擾
  QString getFormattedBitrate() const {
   double br = static_cast<double>(bitrate);
   
   if (br >= 1000000.0) {
    return QString::number(br / 1000000.0, 'f', 2) + " Mbps";
   } else if (br >= 1000.0) {
    return QString::number(br / 1000.0, 'f', 0) + " kbps";
   } else {
    return QString::number(br, 'f', 0) + " bps";
   }
  }
  
  // 𑜓x𕶎Ŏ擾iŏ̃rfIXg[j
  QString getResolutionString() const {
   const auto* video = getFirstVideoStream();
   if (video && video->resolution.isValid()) {
    return QString("%1x%2")
           .arg(video->resolution.width())
           .arg(video->resolution.height());
   }
   return QString();
  }
  
  // t[[g𕶎Ŏ擾
  QString getFrameRateString() const {
   const auto* video = getFirstVideoStream();
   if (video && video->frameRate > 0.0) {
    return QString::number(video->frameRate, 'f', 2) + " fps";
   }
   return QString();
  }
  
  // JSON`ŏo
  QString toJsonString() const;
  
  // fobOp
  QString toString() const {
   QString result;
   result += QString("File: %1\n").arg(fileName);
   result += QString("Duration: %1\n").arg(getFormattedDuration());
   result += QString("Size: %1\n").arg(getFormattedFileSize());
   result += QString("Bitrate: %1\n").arg(getFormattedBitrate());
   
   if (hasVideo()) {
    result += QString("Resolution: %1\n").arg(getResolutionString());
    result += QString("Frame Rate: %1\n").arg(getFrameRateString());
   }
   
   return result;
  }
 };

 // ^f[^̔r
 inline bool operator==(const MediaMetaData& lhs, const MediaMetaData& rhs) {
  return lhs.filePath == rhs.filePath && 
         lhs.duration == rhs.duration;
 }

 inline bool operator!=(const MediaMetaData& lhs, const MediaMetaData& rhs) {
  return !(lhs == rhs);
 }

};
