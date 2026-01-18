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

 class LIBRARY_DLL_API AssetMeta {
 private:
  std::map<UniString, UniString> data;
 public:
  void setValue(const UniString& key, const UniString& value);
  UniString getValue(const UniString& key) const;
  bool hasKey(const UniString& key) const;
  void removeKey(const UniString& key);
  std::vector<UniString> keys() const;
 };

 class LIBRARY_DLL_API AbstractAssetFile :public QObject {
  W_OBJECT(AbstractAssetFile)
 private:
  class Impl;
  Impl* impl_;
 protected:
  virtual bool _load(); // Protected virtual with default implementation
  virtual void _unload(); // Protected virtual with default implementation
 public:
  AbstractAssetFile();
  AbstractAssetFile(const QFile& file);
  AbstractAssetFile(const UniString& path);
  AbstractAssetFile(const AbstractAssetFile&) = delete; // QObject はコピー不可
  AbstractAssetFile& operator=(const AbstractAssetFile&) = delete;
  virtual ~AbstractAssetFile();
  bool exist() const;
  bool notExist() const;
  AssetID assetID() const;
  void setAssetID(const AssetID& assetID);

  AssetMeta& meta();
  const AssetMeta& meta() const;

  void addDependency(const AssetID& dependency);
  std::vector<AssetID> getDependencies() const;

  bool isDirty() const;
  void markDirty();
  void clearDirty();

  bool isLoaded() const;
  bool load(); // Public, calls _load
  void unload(); // Public, calls _unload
  UniString filePath() const;

  //public slots:

 };

 typedef std::shared_ptr<AbstractAssetFile>  AbstractAssetFilePtr;
 typedef MultiIndexContainer<AbstractAssetFilePtr, AssetID> AssetMultiIndexContainer;

};

W_REGISTER_ARGTYPE(ArtifactCore::AssetID)