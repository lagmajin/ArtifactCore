module;
#include <QFile>
#include <QString>
#include <QtCore/QObject>
#include <wobjectdefs.h>
#include "../Define/DllExportMacro.hpp"
export module Asset.File;

//import String.like;
import std;
import Utils.Id;
import Utils.String.UniString;
import Container.MultiIndex;

export namespace ArtifactCore {

 class LIBRARY_DLL_API AssetID : public Id {
 public:
  using Id::Id;
 };


 class LIBRARY_DLL_API AbstractAssetFile :public QObject {
  W_OBJECT(AbstractAssetFile)
 private:
  class Impl;
  Impl* impl_;
 public:
  AbstractAssetFile();
  AbstractAssetFile(const QFile& file);
  AbstractAssetFile(const UniString& path);
  virtual ~AbstractAssetFile();
  bool exist() const;
  bool notExist() const;
  AssetID assetID() const;
  void setAssetID(const AssetID& assetID);


  void setMetaValue();

  void addDependency();
  bool isDirty() const;
  void markDirty();
  void clearDirty();


  bool isLoaded() const;
  bool load();
  void unload();
  UniString filePath() const;

  //public slots:

 };

 typedef std::shared_ptr<AbstractAssetFile>  AbstractAssetFilePtr;
 typedef MultiIndexContainer<AbstractAssetFilePtr, AssetID> AssetMultiIndexContainer;

};

W_REGISTER_ARGTYPE(ArtifactCore::AssetID)