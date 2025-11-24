module;
#include <QString>
export module Utils.Tag;

import std;
import Utils.UniString;
import Utils.String.Like;

export namespace ArtifactCore {

 class Tag final{
 private:
  class Impl;
  Impl* impl_;
 public:
  Tag();
  Tag(const UniString& name);
  ~Tag();

  template<StringLike S>
  void setName(const S& name);
  void setName(const UniString& name);

 };

 template<StringLike S>
 void ArtifactCore::Tag::setName(const S& v)
 {

 }

}