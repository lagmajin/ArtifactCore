module;
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QIODevice>
#include <QString>
#include <QStringList>

module File.TypeDetector;

namespace ArtifactCore {

namespace {

constexpr unsigned int kFlagImport = 1u << 0;
constexpr unsigned int kFlagStill = 1u << 1;
constexpr unsigned int kFlagSequence = 1u << 2;
constexpr unsigned int kFlagExport = 1u << 3;
constexpr unsigned int kFlagSequenceExport = 1u << 4;
constexpr unsigned int kFlagModel3D = 1u << 5;
constexpr unsigned int kFlagVideo = 1u << 6;
constexpr unsigned int kFlagAudio = 1u << 7;

constexpr unsigned int kStillImage = kFlagImport | kFlagStill;
constexpr unsigned int kQtSequenceImage = kStillImage | kFlagSequence;
constexpr unsigned int kOiiImage = kStillImage | kFlagExport |
                                  kFlagSequenceExport;
constexpr unsigned int kOiiSequenceImage = kOiiImage | kFlagSequence;

struct FormatEntry {
  const char *suffix;
  FileType type;
  unsigned int flags;
};

// 一枚の表が分類・取り込みゲート・UI フィルタの共通根拠。
// flags は「コードパスが存在するか」だけを表し、OIIO / Qt plugin の
// runtime 可否は含まない（plugin 依存形式は実呼び出しで検証する）。
const FormatEntry kFormats[] = {
  // 画像。kFlagSequence は QImageReader で連番デコードできる形式だけ。
  {"png", FileType::Image, kOiiSequenceImage},
  {"jpg", FileType::Image, kOiiSequenceImage},
  {"jpeg", FileType::Image, kOiiSequenceImage},
  {"bmp", FileType::Image, kOiiSequenceImage},
  {"gif", FileType::Image, kQtSequenceImage},
  {"tif", FileType::Image, kOiiSequenceImage},
  {"tiff", FileType::Image, kOiiSequenceImage},
  {"webp", FileType::Image, kOiiSequenceImage},
  {"tga", FileType::Image, kOiiImage},
  {"exr", FileType::Image, kOiiImage},
  {"dpx", FileType::Image, kOiiImage},
  {"hdr", FileType::Image, kOiiImage},
  {"psd", FileType::Image, kStillImage},
  {"psb", FileType::Image, kStillImage},
  {"svg", FileType::Image, kFlagImport | kFlagStill | kFlagSequenceExport},
  {"ico", FileType::Image, kQtSequenceImage},
  {"dds", FileType::Image, kStillImage},
  {"ktx", FileType::Image, kStillImage},
  {"ktx2", FileType::Image, kStillImage},
  {"avif", FileType::Image, kStillImage},
  {"heic", FileType::Image, kStillImage},
  {"heif", FileType::Image, kStillImage},
  {"jxl", FileType::Image, kStillImage},
  {"jp2", FileType::Image, kStillImage},
  {"j2k", FileType::Image, kStillImage},
  {"ppm", FileType::Image, kQtSequenceImage},
  {"pgm", FileType::Image, kQtSequenceImage},
  {"pbm", FileType::Image, kQtSequenceImage},
  {"pam", FileType::Image, kStillImage},
  {"pfm", FileType::Image, kStillImage},

  // 動画 / 音声
  {"mp4", FileType::Video, kFlagImport | kFlagVideo},
  {"avi", FileType::Video, kFlagImport | kFlagVideo},
  {"mov", FileType::Video, kFlagImport | kFlagVideo},
  {"mkv", FileType::Video, kFlagImport | kFlagVideo},
  {"wmv", FileType::Video, kFlagImport | kFlagVideo},
  {"flv", FileType::Video, kFlagImport | kFlagVideo},
  {"webm", FileType::Video, kFlagImport | kFlagVideo},
  {"m4v", FileType::Video, kFlagImport | kFlagVideo},
  {"mpeg", FileType::Video, kFlagImport | kFlagVideo},
  {"mpg", FileType::Video, kFlagImport | kFlagVideo},
  {"3gp", FileType::Video, kFlagImport | kFlagVideo},
  {"ts", FileType::Video, kFlagImport | kFlagVideo},
  {"mxf", FileType::Video, kFlagImport | kFlagVideo},
  {"mp3", FileType::Audio, kFlagImport | kFlagAudio},
  {"wav", FileType::Audio, kFlagImport | kFlagAudio},
  {"flac", FileType::Audio, kFlagImport | kFlagAudio},
  {"aac", FileType::Audio, kFlagImport | kFlagAudio},
  {"ogg", FileType::Audio, kFlagImport | kFlagAudio},
  {"wma", FileType::Audio, kFlagImport | kFlagAudio},
  {"m4a", FileType::Audio, kFlagImport | kFlagAudio},
  {"aiff", FileType::Audio, kFlagImport | kFlagAudio},
  {"opus", FileType::Audio, kFlagImport | kFlagAudio},
  {"ac3", FileType::Audio, kFlagImport | kFlagAudio},
  {"dts", FileType::Audio, kFlagImport | kFlagAudio},

  // フォント（分類のみ。取り込みゲートは従来どおり通さない）
  {"ttf", FileType::Font, 0u},
  {"otf", FileType::Font, 0u},
  {"ttc", FileType::Font, 0u},
  {"woff", FileType::Font, 0u},
  {"woff2", FileType::Font, 0u},
  {"eot", FileType::Font, 0u},

  // テキスト / データ
  {"txt", FileType::Text, 0u},
  {"md", FileType::Text, 0u},
  {"json", FileType::Text, kFlagImport},
  {"xml", FileType::Text, 0u},
  {"html", FileType::Text, 0u},
  {"css", FileType::Text, 0u},
  {"js", FileType::Text, 0u},
  {"yaml", FileType::Text, 0u},
  {"yml", FileType::Text, 0u},
  {"toml", FileType::Text, 0u},
  {"ini", FileType::Text, 0u},
  {"cfg", FileType::Text, 0u},
  {"log", FileType::Text, 0u},
  {"csv", FileType::Text, 0u},
  {"srt", FileType::Text, 0u},
  {"vtt", FileType::Text, 0u},
  {"ass", FileType::Text, 0u},

  // 文書 / ベクター
  {"ai", FileType::Document, kFlagImport},
  {"pdf", FileType::Document, kFlagImport},
  {"eps", FileType::Document, kFlagImport},
  {"afdesign", FileType::Document, kFlagImport},
  {"afphoto", FileType::Document, kFlagImport},
  {"afpub", FileType::Document, kFlagImport},
  {"doc", FileType::Document, 0u},
  {"docx", FileType::Document, 0u},
  {"xls", FileType::Document, 0u},
  {"xlsx", FileType::Document, 0u},
  {"ppt", FileType::Document, 0u},
  {"pptx", FileType::Document, 0u},

  // アーカイブ
  {"zip", FileType::Archive, 0u},
  {"lottie", FileType::Archive, kFlagImport},
  {"rar", FileType::Archive, 0u},
  {"7z", FileType::Archive, 0u},
  {"tar", FileType::Archive, 0u},
  {"gz", FileType::Archive, 0u},
  {"bz2", FileType::Archive, 0u},
  {"xz", FileType::Archive, 0u},

  // 3D。blend / dae / abc / pmx は分類のみで backend が無い。
  {"obj", FileType::Model3D, kFlagImport | kFlagModel3D},
  {"fbx", FileType::Model3D, kFlagImport | kFlagModel3D},
  {"gltf", FileType::Model3D, kFlagImport | kFlagModel3D},
  {"glb", FileType::Model3D, kFlagImport | kFlagModel3D},
  {"stl", FileType::Model3D, kFlagImport | kFlagModel3D},
  {"blend", FileType::Model3D, 0u},
  {"dae", FileType::Model3D, 0u},
  {"abc", FileType::Model3D, 0u},
  {"usd", FileType::Model3D, kFlagImport | kFlagModel3D},
  {"usda", FileType::Model3D, kFlagImport | kFlagModel3D},
  {"usdc", FileType::Model3D, kFlagImport | kFlagModel3D},
  {"usdz", FileType::Model3D, kFlagImport | kFlagModel3D},
  {"pmd", FileType::Model3D, kFlagImport | kFlagModel3D},
  {"pmx", FileType::Model3D, 0u},
  {"ply", FileType::Model3D, kFlagImport | kFlagModel3D},
  {"las", FileType::Model3D, kFlagImport | kFlagModel3D},
};

constexpr int kFormatCount = static_cast<int>(sizeof(kFormats) / sizeof(kFormats[0]));

QString extensionKey(const QString &value) {
  QString text = value.trimmed().toLower();
  if (text.isEmpty()) {
    return QString();
  }
  if (text.startsWith(QLatin1Char('.'))) {
    text.remove(0, 1);
  }
  const int dot = text.lastIndexOf(QLatin1Char('.'));
  if (dot >= 0) {
    text = text.mid(dot + 1);
  }
  return text;
}

const FormatEntry *findFormat(const QString &value) {
  const QString key = extensionKey(value);
  if (key.isEmpty()) {
    return nullptr;
  }
  static const QHash<QString, int> index = []() {
    QHash<QString, int> table;
    table.reserve(kFormatCount);
    for (int i = 0; i < kFormatCount; ++i) {
      table.insert(QString::fromLatin1(kFormats[i].suffix), i);
    }
    return table;
  }();
  const auto it = index.constFind(key);
  if (it == index.constEnd()) {
    return nullptr;
  }
  return &kFormats[it.value()];
}

unsigned int capabilityFlag(FileCapability capability) {
  switch (capability) {
  case FileCapability::Import: return kFlagImport;
  case FileCapability::StillImage: return kFlagStill;
  case FileCapability::ImageSequence: return kFlagSequence;
  case FileCapability::ImageExport: return kFlagExport;
  case FileCapability::SequenceExport: return kFlagSequenceExport;
  case FileCapability::Model3D: return kFlagModel3D;
  case FileCapability::VideoImport: return kFlagVideo;
  case FileCapability::AudioImport: return kFlagAudio;
  }
  return 0u;
}

} // namespace

class FileTypeDetector::Impl {
public:
  Impl();
  ~Impl();

  FileType detectByExtension(const QString &filePath) const;
  FileType detectByMagicNumber(const QString &filePath) const;
};

FileTypeDetector::Impl::Impl() {}

FileTypeDetector::Impl::~Impl() {}

FileType
FileTypeDetector::Impl::detectByExtension(const QString &filePath) const {
  const FormatEntry *entry = findFormat(filePath);
  return entry ? entry->type : FileType::Unknown;
}

FileType
FileTypeDetector::Impl::detectByMagicNumber(const QString &filePath) const {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return FileType::Unknown;
  }

  QByteArray header = file.read(16); // Read first 16 bytes
  file.close();

  if (header.size() < 4) {
    return FileType::Unknown;
  }

  // PNG
  if (header.startsWith("\x89PNG")) {
    return FileType::Image;
  }
  // Photoshop PSD/PSB
  if (header.startsWith("8BPS")) {
    return FileType::Image;
  }
  // JPEG
  if (header.startsWith("\xFF\xD8\xFF")) {
    return FileType::Image;
  }
  // GIF
  if (header.startsWith("GIF8")) {
    return FileType::Image;
  }
  // BMP
  if (header.startsWith("BM")) {
    return FileType::Image;
  }
  // MP4
  if (header.mid(4, 4) == "ftyp") {
    return FileType::Video;
  }
  // AVI
  if (header.startsWith("RIFF") && header.mid(8, 4) == "AVI ") {
    return FileType::Video;
  }
  // MP3
  if (header.startsWith("ID3") || header.startsWith("\xFF\xFB") ||
      header.startsWith("\xFF\xF3") || header.startsWith("\xFF\xF2")) {
    return FileType::Audio;
  }
  // WAV
  if (header.startsWith("RIFF") && header.mid(8, 4) == "WAVE") {
    return FileType::Audio;
  }
  // TrueType, OpenType, Web Open Font Format
  if ((header.size() >= 4 && header.left(4) == QByteArray::fromHex("00010000")) ||
      header.startsWith("OTTO") || header.startsWith("wOFF") ||
      header.startsWith("wOF2")) {
    return FileType::Font;
  }
  // ZIP
  if (header.startsWith("PK\x03\x04")) {
    return FileType::Archive;
  }
  // RAR
  if (header.startsWith("Rar!")) {
    return FileType::Archive;
  }
  // PDF
  if (header.startsWith("%PDF")) {
    return FileType::Document;
  }
  // Adobe Illustrator AI files often carry a PDF header when saved as PDF-compatible AI.
  if (header.startsWith("%!PS-Adobe") || header.startsWith("%!PS")) {
    return FileType::Document;
  }
  // GLB (Binary glTF)
  if (header.startsWith("glTF")) {
    return FileType::Model3D;
  }
  // PMD (MikuMikuDance Model)
  if (header.startsWith("Pmd")) {
    return FileType::Model3D;
  }
  // Text (simple check for ASCII)
  bool isText = true;
  for (char c : header) {
    if (c < 32 && c != 9 && c != 10 && c != 13) { // Allow tab, LF, CR
      isText = false;
      break;
    }
  }
  if (isText) {
    return FileType::Text;
  }

  return FileType::Binary;
}

FileTypeDetector::FileTypeDetector() : impl_(new Impl()) {}

FileTypeDetector::~FileTypeDetector() { delete impl_; }

FileType FileTypeDetector::detect(const QString &filePath) const {
  FileType extType = impl_->detectByExtension(filePath);
  FileType magicType = impl_->detectByMagicNumber(filePath);

  // Preserve known semantic types from the extension. Many 3D and container
  // formats do not expose a stable magic number and would otherwise regress to
  // Binary/Text despite the UI intentionally treating them as importable media.
  switch (extType) {
  case FileType::Model3D:
  case FileType::Video:
  case FileType::Audio:
  case FileType::Archive:
    return extType;
  default:
    break;
  }

  if (magicType != FileType::Unknown && magicType != FileType::Binary) {
    return magicType;
  }
  return extType;
}

FileType FileTypeDetector::detectByExtension(const QString &filePath) const {
  return impl_->detectByExtension(filePath);
}

FileType FileTypeDetector::detectByMagicNumber(const QString &filePath) const {
  return impl_->detectByMagicNumber(filePath);
}

bool FileTypeDetector::supports(const QString &extension,
                                FileCapability capability) {
  const FormatEntry *entry = findFormat(extension);
  return entry && (entry->flags & capabilityFlag(capability)) != 0u;
}

QString FileTypeDetector::suffixPatterns(FileCapability capability) {
  const unsigned int flag = capabilityFlag(capability);
  if (flag == 0u) {
    return QString();
  }
  QStringList patterns;
  patterns.reserve(kFormatCount);
  for (int i = 0; i < kFormatCount; ++i) {
    if ((kFormats[i].flags & flag) != 0u) {
      patterns.append(QStringLiteral("*.%1").arg(
          QLatin1String(kFormats[i].suffix)));
    }
  }
  return patterns.join(QLatin1Char(' '));
}

} // namespace ArtifactCore
