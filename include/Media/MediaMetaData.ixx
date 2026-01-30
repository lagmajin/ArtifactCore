module;

#include <QString>
#include <QDateTime>
#include <QSize>
#include <QStringList>
#include <QMap>

export module Media.MetaData;

import std;

export namespace ArtifactCore {

 // メディアタイプ
 enum class MediaType {
  Unknown,
  Video,
  Audio,
  Image,
  Subtitle,
  Data
 };

 // ビデオコーデック情報
 struct VideoCodecInfo {
  QString codecName;              // コーデック名（例：h264, hevc, vp9）
  QString codecLongName;          // 詳細名（例：H.264 / AVC / MPEG-4 AVC）
  QString profile;                // プロファイル（例：High, Main）
  QString level;                  // レベル（例：4.0, 5.1）
  int bitrate = 0;                // ビットレート（bps）
  
  QString pixelFormat;            // ピクセルフォーマット（例：yuv420p）
  QString colorSpace;             // 色空間（例：bt709, bt2020）
  QString colorRange;             // 色範囲（例：tv, pc）
  
  bool hasAlpha = false;          // アルファチャンネルの有無
  int bitsPerComponent = 8;       // コンポーネントあたりのビット数
 };

 // オーディオコーデック情報
 struct AudioCodecInfo {
  QString codecName;              // コーデック名（例：aac, mp3, opus）
  QString codecLongName;          // 詳細名
  int bitrate = 0;                // ビットレート（bps）
  int sampleRate = 0;             // サンプルレート（Hz）
  int channels = 0;               // チャンネル数
  QString channelLayout;          // チャンネルレイアウト（例：stereo, 5.1）
  QString sampleFormat;           // サンプルフォーマット（例：fltp, s16）
  int bitsPerSample = 0;          // サンプルあたりのビット数
 };

 // ストリーム情報
 struct StreamInfo {
  int index = -1;                 // ストリームインデックス
  MediaType type = MediaType::Unknown;
  QString language;               // 言語（例：jpn, eng）
  QString title;                  // タイトル
  bool isDefault = false;         // デフォルトストリーム
  bool isForced = false;          // 強制表示
  
  // ビデオストリーム固有
  QSize resolution;               // 解像度
  double frameRate = 0.0;         // フレームレート
  double aspectRatio = 0.0;       // アスペクト比
  int64_t frameCount = 0;         // 総フレーム数
  VideoCodecInfo videoCodec;
  
  // オーディオストリーム固有
  AudioCodecInfo audioCodec;
  
  // 共通
  double duration = 0.0;          // 秒
  int64_t bitrate = 0;            // ビットレート（bps）
  QMap<QString, QString> metadata; // その他のメタデータ
 };

 // チャプター情報
 struct ChapterInfo {
  int id = 0;
  double startTime = 0.0;         // 秒
  double endTime = 0.0;           // 秒
  QString title;
  QMap<QString, QString> metadata;
 };

 // メディアメタデータ
 struct MediaMetaData {
  // ---- ファイル情報 ----
  QString filePath;               // ファイルパス
  QString fileName;               // ファイル名
  QString fileExtension;          // 拡張子
  int64_t fileSize = 0;           // ファイルサイズ（バイト）
  
  QString formatName;             // フォーマット名（例：mp4, mkv, avi）
  QString formatLongName;         // フォーマット詳細名
  
  // ---- 基本メタデータ ----
  QString title;                  // タイトル
  QString artist;                 // アーティスト/作者
  QString album;                  // アルバム
  QString albumArtist;            // アルバムアーティスト
  QString composer;               // 作曲者
  QString genre;                  // ジャンル
  QString year;                   // 年
  QString date;                   // 日付
  int track = 0;                  // トラック番号
  int totalTracks = 0;            // 総トラック数
  int disc = 0;                   // ディスク番号
  int totalDiscs = 0;             // 総ディスク数
  
  QString comment;                // コメント
  QString description;            // 説明
  QString copyright;              // 著作権
  QString publisher;              // 発行者
  QString encodedBy;              // エンコードした人/ツール
  
  QStringList keywords;           // キーワード/タグ
  
  // ---- 技術情報 ----
  double duration = 0.0;          // 総デュレーション（秒）
  int64_t bitrate = 0;            // 総ビットレート（bps）
  QDateTime creationTime;         // 作成日時
  QDateTime modificationTime;     // 変更日時
  
  // ---- ストリーム情報 ----
  std::vector<StreamInfo> streams;
  
  // ビデオストリームのショートカット
  bool hasVideo() const {
   return std::any_of(streams.begin(), streams.end(), 
                      [](const StreamInfo& s) { return s.type == MediaType::Video; });
  }
  
  // オーディオストリームのショートカット
  bool hasAudio() const {
   return std::any_of(streams.begin(), streams.end(), 
                      [](const StreamInfo& s) { return s.type == MediaType::Audio; });
  }
  
  // 字幕ストリームのショートカット
  bool hasSubtitles() const {
   return std::any_of(streams.begin(), streams.end(), 
                      [](const StreamInfo& s) { return s.type == MediaType::Subtitle; });
  }
  
  // 最初のビデオストリームを取得
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
  
  // 最初のオーディオストリームを取得
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
  
  // ---- チャプター情報 ----
  std::vector<ChapterInfo> chapters;
  
  // ---- カスタムメタデータ ----
  QMap<QString, QString> customMetadata;
  
  // ---- 統計情報 ----
  struct Statistics {
   int64_t totalFrames = 0;
   int64_t droppedFrames = 0;
   double averageBitrate = 0.0;
   double maxBitrate = 0.0;
   double minBitrate = 0.0;
  };
  Statistics statistics;
  
  // ---- サムネイル/カバーアート ----
  QString thumbnailPath;          // サムネイル画像のパス
  QByteArray coverArt;            // カバーアート画像データ
  QString coverArtMimeType;       // カバーアートのMIMEタイプ
  
  // ---- ユーティリティ ----
  
  // メタデータが有効かチェック
  bool isValid() const {
   return !filePath.isEmpty() && duration > 0.0;
  }
  
  // 人間が読める形式でデュレーションを取得
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
  
  // 人間が読める形式でファイルサイズを取得
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
  
  // 人間が読める形式でビットレートを取得
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
  
  // 解像度を文字列で取得（最初のビデオストリームから）
  QString getResolutionString() const {
   const auto* video = getFirstVideoStream();
   if (video && video->resolution.isValid()) {
    return QString("%1x%2")
           .arg(video->resolution.width())
           .arg(video->resolution.height());
   }
   return QString();
  }
  
  // フレームレートを文字列で取得
  QString getFrameRateString() const {
   const auto* video = getFirstVideoStream();
   if (video && video->frameRate > 0.0) {
    return QString::number(video->frameRate, 'f', 2) + " fps";
   }
   return QString();
  }
  
  // JSON形式で出力
  QString toJsonString() const;
  
  // デバッグ用文字列
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

 // メタデータの比較
 inline bool operator==(const MediaMetaData& lhs, const MediaMetaData& rhs) {
  return lhs.filePath == rhs.filePath && 
         lhs.duration == rhs.duration;
 }

 inline bool operator!=(const MediaMetaData& lhs, const MediaMetaData& rhs) {
  return !(lhs == rhs);
 }

};