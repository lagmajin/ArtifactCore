module;

#include <QString>
#include <QJsonDocument>
#include <QDateTime>

#include <optional>

#include "../Define/DllExportMacro.hpp"

export module Media.Info;



export namespace ArtifactCore {

 class MediaInfoBuilder;


 class LIBRARY_DLL_API MediaInfo {
 private:
  class Impl;
  Impl* impl_;
  friend class MediaInfoBuilder;

 public:
  MediaInfo();
  MediaInfo(QString title,int width, int height,long long duration,QString codec);
  ~MediaInfo();
  int width() const;
  int height() const;
  long long duration() const;
  QString title() const;        // もし必要なら追加
  QString codecName() const;
  //void clear();

 };

 class LIBRARY_DLL_API MediaInfoBuilder {
 private:
  class Impl;
  Impl* impl_;
 public:
  MediaInfoBuilder& setWidth(int w);
  MediaInfoBuilder& setHeight(int h);
  MediaInfoBuilder& setTitle(const QString& title);
  MediaInfoBuilder& setCreationTime(const QDateTime& dt);
  MediaInfoBuilder& setBitrate(int br);
  MediaInfoBuilder& setDuration(int64_t duration);

  MediaInfo build() const;
 };





};