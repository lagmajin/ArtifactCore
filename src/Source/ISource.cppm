module;

#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QUrl>
#include <utility>

module Source.ISource;

namespace ArtifactCore {

namespace {

static SourceMetadata makeFileMetadata(const QFileInfo& info, const QString& displayNameOverride = QString()) {
 SourceMetadata metadata;
 metadata.displayName = displayNameOverride.isEmpty() ? info.fileName() : displayNameOverride;
 metadata.uri = QUrl::fromLocalFile(info.absoluteFilePath()).toString();
 metadata.fileExtension = info.suffix().toLower();
 metadata.byteSize = info.exists() ? info.size() : -1;
 metadata.lastModified = info.exists() ? info.lastModified() : QDateTime();
 metadata.isDirectory = info.isDir();
 metadata.isLocalFile = true;

 QMimeDatabase db;
 metadata.mimeType = db.mimeTypeForFile(info).name();
 if (metadata.mimeType.isEmpty()) {
  metadata.mimeType = QStringLiteral("application/octet-stream");
 }

 return metadata;
}

static SourceMetadata makeGeneratedMetadata(const QString& displayName,
                                            const QString& uri,
                                            const QString& mimeType,
                                            const QSize& sizeHint,
                                            qint64 byteSize) {
 SourceMetadata metadata;
 metadata.displayName = displayName;
 metadata.uri = uri;
 metadata.mimeType = mimeType.isEmpty() ? QStringLiteral("application/octet-stream") : mimeType;
 metadata.byteSize = byteSize;
 metadata.sizeHint = sizeHint;
 metadata.isDirectory = false;
 metadata.isLocalFile = false;
 return metadata;
}

} // namespace

ISource::~ISource() = default;

FileSource::FileSource(const QString& filePath)
 : id_()
 , filePath_(filePath) {
 metadata_ = makeFileMetadata(QFileInfo(filePath_));
 reload();
}

FileSource::~FileSource() = default;

SourceKind FileSource::kind() const {
 return SourceKind::File;
}

const Id& FileSource::sourceId() const {
 return id_;
}

QString FileSource::displayName() const {
 return metadata_.displayName;
}

QString FileSource::uri() const {
 return metadata_.uri;
}

bool FileSource::isValid() const {
 return !filePath_.isEmpty();
}

bool FileSource::exists() const {
 return QFileInfo::exists(filePath_);
}

SourceCapability FileSource::capabilities() const {
 SourceCapability caps = SourceCapability::Read;
 caps |= SourceCapability::Reload;
 caps |= SourceCapability::Relink;
 caps |= SourceCapability::Binary;
 return caps;
}

SourceMetadata FileSource::metadata() const {
 return metadata_;
}

QByteArray FileSource::readAll() const {
 if (!cacheValid_) {
  const_cast<FileSource*>(this)->reload();
 }
 return cachedData_;
}

bool FileSource::reload() {
 error_.clear();
 cachedData_.clear();
 cacheValid_ = false;

 if (filePath_.isEmpty()) {
  error_ = QStringLiteral("file path is empty");
  metadata_ = makeFileMetadata(QFileInfo(), QString());
  return false;
 }

 QFile file(filePath_);
 if (!file.open(QIODevice::ReadOnly)) {
  error_ = file.errorString();
  metadata_ = makeFileMetadata(QFileInfo(filePath_));
  return false;
 }

 cachedData_ = file.readAll();
 cacheValid_ = true;

 QFileInfo info(filePath_);
 metadata_ = makeFileMetadata(info);
 metadata_.byteSize = cachedData_.size();
 return true;
}

bool FileSource::relink(const QString& newUri) {
 if (newUri.isEmpty()) {
  error_ = QStringLiteral("new uri is empty");
  return false;
 }

 QFileInfo info(newUri);
 if (!info.exists()) {
  error_ = QStringLiteral("relink target does not exist");
  filePath_ = newUri;
  metadata_ = makeFileMetadata(info);
  cacheValid_ = false;
  return false;
 }

 filePath_ = newUri;
 metadata_ = makeFileMetadata(info);
 return reload();
}

QString FileSource::errorString() const {
 return error_;
}

const QString& FileSource::filePath() const {
 return filePath_;
}

void FileSource::setFilePath(const QString& path) {
 filePath_ = path;
 metadata_ = makeFileMetadata(QFileInfo(filePath_));
 cacheValid_ = false;
 error_.clear();
}

GeneratedSource::GeneratedSource(QString displayName,
                                 QByteArray initialData,
                                 QString mimeType,
                                 QSize sizeHint,
                                 Generator generator)
 : id_()
 , displayName_(std::move(displayName))
 , uri_(QStringLiteral("generated://") + id_.toString())
 , mimeType_(std::move(mimeType))
 , sizeHint_(sizeHint)
 , cachedData_(std::move(initialData))
 , generator_(std::move(generator))
 , cacheValid_(true) {
 if (displayName_.isEmpty()) {
  displayName_ = QStringLiteral("GeneratedSource");
 }
 metadata_ = makeGeneratedMetadata(displayName_, uri_, mimeType_, sizeHint_, cachedData_.size());
}

GeneratedSource::~GeneratedSource() = default;

SourceKind GeneratedSource::kind() const {
 return SourceKind::Generated;
}

const Id& GeneratedSource::sourceId() const {
 return id_;
}

QString GeneratedSource::displayName() const {
 return displayName_;
}

QString GeneratedSource::uri() const {
 return uri_;
}

bool GeneratedSource::isValid() const {
 return !displayName_.isEmpty();
}

bool GeneratedSource::exists() const {
 return cacheValid_ || static_cast<bool>(generator_);
}

SourceCapability GeneratedSource::capabilities() const {
 SourceCapability caps = SourceCapability::Read;
 caps |= SourceCapability::Reload;
 caps |= SourceCapability::Mutable;
 caps |= SourceCapability::Binary;
 return caps;
}

SourceMetadata GeneratedSource::metadata() const {
 return metadata_;
}

QByteArray GeneratedSource::readAll() const {
 if (!cacheValid_) {
  const_cast<GeneratedSource*>(this)->reload();
 }
 return cachedData_;
}

bool GeneratedSource::reload() {
 error_.clear();

 if (generator_) {
  cachedData_ = generator_();
  cacheValid_ = true;
 }
 else {
  cacheValid_ = true;
 }

 metadata_ = makeGeneratedMetadata(displayName_, uri_, mimeType_, sizeHint_, cachedData_.size());
 return true;
}

bool GeneratedSource::relink(const QString& newUri) {
 if (newUri.isEmpty()) {
  error_ = QStringLiteral("new uri is empty");
  return false;
 }

 uri_ = newUri;
 metadata_ = makeGeneratedMetadata(displayName_, uri_, mimeType_, sizeHint_, cachedData_.size());
 return true;
}

QString GeneratedSource::errorString() const {
 return error_;
}

void GeneratedSource::setDisplayName(const QString& displayName) {
 displayName_ = displayName.isEmpty() ? QStringLiteral("GeneratedSource") : displayName;
 metadata_.displayName = displayName_;
}

void GeneratedSource::setMimeType(const QString& mimeType) {
 mimeType_ = mimeType.isEmpty() ? QStringLiteral("application/octet-stream") : mimeType;
 metadata_.mimeType = mimeType_;
}

void GeneratedSource::setSizeHint(const QSize& sizeHint) {
 sizeHint_ = sizeHint;
 metadata_.sizeHint = sizeHint_;
}

void GeneratedSource::setData(const QByteArray& data) {
 cachedData_ = data;
 cacheValid_ = true;
 metadata_.byteSize = cachedData_.size();
}

void GeneratedSource::setGenerator(Generator generator) {
 generator_ = std::move(generator);
 cacheValid_ = false;
}

void GeneratedSource::clearGenerator() {
 generator_ = {};
}

std::shared_ptr<ISource> makeFileSource(const QString& filePath) {
 return std::make_shared<FileSource>(filePath);
}

std::shared_ptr<ISource> makeGeneratedSource(QString displayName,
                                             QByteArray initialData,
                                             QString mimeType,
                                             QSize sizeHint,
                                             GeneratedSource::Generator generator) {
 return std::make_shared<GeneratedSource>(std::move(displayName),
                                          std::move(initialData),
                                          std::move(mimeType),
                                          sizeHint,
                                          std::move(generator));
}

} // namespace ArtifactCore
