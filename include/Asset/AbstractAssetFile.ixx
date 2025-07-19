module;
#include <QFile>
#include <QtCore/QObject>


#include <wobjectdefs.h>
export module Asset;





export namespace ArtifactCore {

 class AbstractAssetFilePrivate;

 class AbstractAssetFile :public QObject{
 W_OBJECT(AbstractAssetFile)
 private:
  class Impl;
  Impl* impl_;
 public:
  AbstractAssetFile();
  AbstractAssetFile(const QFile& file);
  virtual ~AbstractAssetFile();
  bool exist() const;
  bool notExist() const;
 //public slots:
  
 };



}