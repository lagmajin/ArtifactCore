module;

#include <QtCore/QString>
#include <QtCore/QJsonDocument>
export module MediaInfo;



export namespace ArtifactCore {

 class MediaInfoPrivate;

 class MediaInfo {
 private:

 public:
  MediaInfo();
  ~MediaInfo();
  int width() const;
  int height() const;
  long long duration() const;

  //void clear();

 };








};