module;
#include <QString>
export module Utils.Tag;

import std;
import Utils.UniString;
import Utils.String.Like;

export namespace ArtifactCore {

 class Tag {
 private:
  class Impl;
  Impl* impl_;
 public:
  Tag();
  Tag(const UniString& name);
  ~Tag();

 };


}