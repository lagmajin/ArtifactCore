module;
//#include "../../include/Asset/AbstractAssetFile.hpp"

#include <QFile>
#include <wobjectimpl.h>
//#include <wobjectimpl.h>

module Asset.File;
import std;
import Utils.Id;
import Utils.String.UniString;

namespace ArtifactCore {

 W_OBJECT_IMPL(AbstractAssetFile)

 // AssetMeta implementation
 void AssetMeta::setValue(const UniString& key, const UniString& value) {
  data[key] = value;
 }

 UniString AssetMeta::getValue(const UniString& key) const {
  auto it = data.find(key);
  if (it != data.end()) {
    return it->second;
  }
  return UniString();
 }

 bool AssetMeta::hasKey(const UniString& key) const {
  return data.find(key) != data.end();
 }

 void AssetMeta::removeKey(const UniString& key) {
  data.erase(key);
 }

 std::vector<UniString> AssetMeta::keys() const {
  std::vector<UniString> result;
  for (const auto& pair : data) {
    result.push_back(pair.first);
  }
  return result;
 }

  class  AbstractAssetFile::Impl {
  private:

  public:
   Impl();
   explicit Impl(const UniString& str);
   ~Impl();
   UniString path;
   AssetID id;
   bool loaded = false;
   bool dirty = false;
   AssetMeta meta;
   std::vector<AssetID> dependencies;
 };

 AbstractAssetFile::Impl::Impl()
 {

 }

 AbstractAssetFile::Impl::Impl(const UniString& str) : path(str)
 {

 }

 AbstractAssetFile::Impl::~Impl()
 {

 }

 AbstractAssetFile::AbstractAssetFile() :impl_(new Impl())
 {

 }

 AbstractAssetFile::AbstractAssetFile(const QFile& file) :impl_(new Impl(UniString(file.fileName())))
 {

 }

 AbstractAssetFile::AbstractAssetFile(const UniString& path) :impl_(new Impl(path))
 {

 }

 AbstractAssetFile::~AbstractAssetFile()
 {
  delete impl_;
 }

 bool AbstractAssetFile::exist() const
 {
  QFile file(impl_->path.toQString());
  return file.exists();
 }

 bool AbstractAssetFile::notExist() const
 {
  return !exist();
 }

 AssetID AbstractAssetFile::assetID() const
 {
  return impl_->id;
 }

 void AbstractAssetFile::setAssetID(const AssetID& assetID)
 {
  impl_->id = assetID;
 }

 AssetMeta& AbstractAssetFile::meta()
 {
  return impl_->meta;
 }

 const AssetMeta& AbstractAssetFile::meta() const
 {
  return impl_->meta;
 }

 void AbstractAssetFile::addDependency(const AssetID& dependency)
 {
  impl_->dependencies.push_back(dependency);
  impl_->dirty = true;
 }

 std::vector<AssetID> AbstractAssetFile::getDependencies() const
 {
  return impl_->dependencies;
 }

 void AbstractAssetFile::markDirty()
 {
  impl_->dirty = true;
 }

 void AbstractAssetFile::clearDirty()
 {
  impl_->dirty = false;
 }

 bool AbstractAssetFile::isDirty() const
 {
  return impl_->dirty;
 }

 bool AbstractAssetFile::isLoaded() const
 {
  return impl_->loaded;
 }

 bool AbstractAssetFile::load()
 {
  if (impl_->loaded) return true;
  bool result = _load();
  if (result) {
    impl_->loaded = true;
  }
  return result;
 }

 void AbstractAssetFile::unload()
 {
  if (!impl_->loaded) return;
  _unload();
  impl_->loaded = false;
 }

 UniString AbstractAssetFile::filePath() const
 {
  return impl_->path;
 }

 // Default implementations for _load and _unload
 bool AbstractAssetFile::_load()
 {
  // Default: do nothing, subclasses override
  return true;
 }

 void AbstractAssetFile::_unload()
 {
  // Default: do nothing, subclasses override
 }

};