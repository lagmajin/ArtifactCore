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


  class  AbstractAssetFile::Impl {
  private:

  public:
   Impl();
   explicit Impl(const UniString& str);
   ~Impl();
   UniString path;
   AssetID id;
 };

 AbstractAssetFile::Impl::Impl()
 {

 }

 AbstractAssetFile::Impl::Impl(const UniString& str)
 {

 }

 AbstractAssetFile::Impl::~Impl()
 {

 }

 AbstractAssetFile::AbstractAssetFile() :impl_(new Impl())
 {

 }

 AbstractAssetFile::AbstractAssetFile(const QFile& file) :impl_(new Impl())
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
  return true;
 }

 bool AbstractAssetFile::notExist() const
 {
  return !exist();
 }

 AssetID AbstractAssetFile::assetID() const
 {
  return AssetID();
 }

 void AbstractAssetFile::markDirty()
 {

 }

 void AbstractAssetFile::clearDirty()
 {

 }

 bool AbstractAssetFile::load()
 {
  return true;
 }

 void AbstractAssetFile::unload()
 {

 }

 bool AbstractAssetFile::isLoaded() const
 {
  return false;
 }

 void AbstractAssetFile::setAssetID(const AssetID& assetID)
 {

 }

 bool AbstractAssetFile::isDirty() const
 {
  return true;
 }

 UniString AbstractAssetFile::filePath() const
 {
  return impl_->path;
 }

};