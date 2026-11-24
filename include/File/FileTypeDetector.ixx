module;
#include <QString>

export module File.TypeDetector;

export namespace ArtifactCore {

 enum class FileType {
  Unknown,
  Image,
  Video,
  Audio,
  Text,
  Font,
  Binary,
  Document,
  Archive,
  Model3D
 };

 /**
  * 拡張子から静的に決まる形式の能力。
  * 分類（FileType）とは別に、取り込みゲートや UI フィルタの共通根拠となる。
  * runtime のプラグイン可否は含まれない（未確認形式は実呼び出しで検証する）。
  */
 enum class FileCapability {
  Import,         // アセット取り込みゲートを通る
  StillImage,     // 単体画像レイヤーとして扱える
  ImageSequence,  // 連番フレームとしてデコードできる
  ImageExport,    // 単体画像として書き出せる
  SequenceExport, // 連番として書き出せる
  Model3D,        // 3D モデルとして取り込める
  VideoImport,    // 動画として取り込める
  AudioImport     // 音声として取り込める
 };

 class FileTypeDetector {
 private:
  class Impl;
  Impl* impl_;
 public:
  FileTypeDetector();
  ~FileTypeDetector();

  FileType detect(const QString& filePath) const;
  FileType detectByExtension(const QString& filePath) const;
  FileType detectByMagicNumber(const QString& filePath) const;

  /** 拡張子（".png" と "png" とパスのいずれも可）が能力を持つか。 */
  static bool supports(const QString& extension, FileCapability capability);

  /** 指定能力の拡張子を "*.png *.jpg" 形式で返す（ダイアログ用）。 */
  static QString suffixPatterns(FileCapability capability);
 };

}
