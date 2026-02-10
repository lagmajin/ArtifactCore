module;
#include <QDir>
#include <QRegularExpression>

module Graphics.PSO.Cache;

import Utils.Path;
import Utils.String.UniString;

namespace ArtifactCore
{
 namespace
 {
  QString sanitizePathPart(const QString& value)
  {
   QString sanitized = value;
   sanitized.replace(QRegularExpression(R"([<>:"/\\|?*])"), "_");
   sanitized = sanitized.trimmed();
   if (sanitized.isEmpty()) {
    sanitized = "Unknown";
   }
   return sanitized;
  }
 }

 UniString getPSOCacheDirectory(const UniString& vendor,
                               const UniString& deviceName,
                               const UniString& cacheFolder,
                               bool createIfMissing)
 {
  QString vendorPart = sanitizePathPart(vendor.toQString());
  QString devicePart = sanitizePathPart(deviceName.toQString());
  QString cachePart = sanitizePathPart(cacheFolder.toQString());

  QDir base(getAppPath());
  QString path = base.filePath(vendorPart + "/" + devicePart + "/" + cachePart);
  path = QDir::cleanPath(path);

  if (createIfMissing) {
   QDir dir;
   dir.mkpath(path);
  }

  UniString result;
  result.setQString(path);
  return result;
 }
}
