module;
#include <utility>

#include "../Define/DllExportMacro.hpp"

#include <functional>
#include <memory>
#include <QByteArray>
#include <QDateTime>
#include <QSize>
#include <QString>
#include <QStringList>
export module Source.ISource;

import Utils.Id;

export namespace ArtifactCore {

enum class SourceKind : quint8 {
 Unknown = 0,
 File,
 Generated,
};

enum class SourceCapability : quint32 {
 None = 0,
 Read = 1u << 0,
 Reload = 1u << 1,
 Relink = 1u << 2,
 Mutable = 1u << 3,
 Binary = 1u << 4,
 Text = 1u << 5,
};

constexpr SourceCapability operator|(SourceCapability lhs, SourceCapability rhs) {
 return static_cast<SourceCapability>(
  static_cast<quint32>(lhs) | static_cast<quint32>(rhs));
}

constexpr SourceCapability operator&(SourceCapability lhs, SourceCapability rhs) {
 return static_cast<SourceCapability>(
  static_cast<quint32>(lhs) & static_cast<quint32>(rhs));
}

constexpr SourceCapability& operator|=(SourceCapability& lhs, SourceCapability rhs) {
 lhs = lhs | rhs;
 return lhs;
}

constexpr bool hasCapability(SourceCapability capabilities, SourceCapability capability) {
 return (static_cast<quint32>(capabilities) & static_cast<quint32>(capability)) != 0;
}

struct LIBRARY_DLL_API SourceMetadata {
 QString displayName;
 QString uri;
 QString mimeType;
 QString fileExtension;
 qint64 byteSize = -1;
 QDateTime lastModified;
 QSize sizeHint;
 QStringList tags;
 bool isDirectory = false;
 bool isLocalFile = true;
};

class LIBRARY_DLL_API ISource {
public:
 virtual ~ISource();

 virtual SourceKind kind() const = 0;
 virtual const Id& sourceId() const = 0;
 virtual QString displayName() const = 0;
 virtual QString uri() const = 0;
 virtual bool isValid() const = 0;
 virtual bool exists() const = 0;
 virtual SourceCapability capabilities() const = 0;
 virtual SourceMetadata metadata() const = 0;
 virtual QByteArray readAll() const = 0;
 virtual bool reload() = 0;
 virtual bool relink(const QString& newUri) = 0;
 virtual QString errorString() const = 0;
};

class LIBRARY_DLL_API FileSource final : public ISource {
public:
 explicit FileSource(const QString& filePath = QString());
 ~FileSource() override;

 SourceKind kind() const override;
 const Id& sourceId() const override;
 QString displayName() const override;
 QString uri() const override;
 bool isValid() const override;
 bool exists() const override;
 SourceCapability capabilities() const override;
 SourceMetadata metadata() const override;
 QByteArray readAll() const override;
 bool reload() override;
 bool relink(const QString& newUri) override;
 QString errorString() const override;

 const QString& filePath() const;
 void setFilePath(const QString& path);

private:
 Id id_;
 QString filePath_;
 mutable QByteArray cachedData_;
 mutable bool cacheValid_ = false;
 mutable QString error_;
 SourceMetadata metadata_;
};

class LIBRARY_DLL_API GeneratedSource final : public ISource {
public:
 using Generator = std::function<QByteArray()>;

  explicit GeneratedSource(QString displayName = QString(),
                         QByteArray initialData = QByteArray(),
                         QString mimeType = QString(),
                         QSize sizeHint = QSize(),
                         Generator generator = {});
 ~GeneratedSource() override;

 SourceKind kind() const override;
 const Id& sourceId() const override;
 QString displayName() const override;
 QString uri() const override;
 bool isValid() const override;
 bool exists() const override;
 SourceCapability capabilities() const override;
 SourceMetadata metadata() const override;
 QByteArray readAll() const override;
 bool reload() override;
 bool relink(const QString& newUri) override;
 QString errorString() const override;

 void setDisplayName(const QString& displayName);
 void setMimeType(const QString& mimeType);
 void setSizeHint(const QSize& sizeHint);
 void setData(const QByteArray& data);
 void setGenerator(Generator generator);
 void clearGenerator();

private:
 Id id_;
 QString displayName_;
 QString uri_;
 QString mimeType_;
 QSize sizeHint_;
 mutable QByteArray cachedData_;
 Generator generator_;
 mutable bool cacheValid_ = false;
 mutable QString error_;
 SourceMetadata metadata_;
};

LIBRARY_DLL_API std::shared_ptr<ISource> makeFileSource(const QString& filePath);
LIBRARY_DLL_API std::shared_ptr<ISource> makeGeneratedSource(QString displayName = QString(),
                                                            QByteArray initialData = QByteArray(),
                                                            QString mimeType = QString(),
                                                            QSize sizeHint = QSize(),
                                                            GeneratedSource::Generator generator = {});

} // namespace ArtifactCore
