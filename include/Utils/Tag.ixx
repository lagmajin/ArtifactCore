module;
#include <QString>
export module Utils.Tag;

import std;
import Utils.String.UniString;
import Utils.String.Like;

export namespace ArtifactCore {

 class Tag final{
 private:
  class Impl;
  Impl* impl_;
 public:
  Tag();
  Tag(const Tag& other);
  Tag(Tag&& other) noexcept;
  Tag(const UniString& name);
  ~Tag();
  UniString name() const;

  template<StringLike S>
  void setName(const S& name);
  void setName(const UniString& name);
  Tag& operator=(const Tag& other);
  Tag& operator=(Tag&& other) noexcept;
 };

 template<StringLike S>
 void ArtifactCore::Tag::setName(const S& v)
 {

 }

}