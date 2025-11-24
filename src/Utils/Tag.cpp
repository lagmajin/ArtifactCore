module;


module Utils.Tag;


import std;
import Utils.String.UniString;

namespace ArtifactCore {

 class Tag::Impl {
 private:

 public:
  Impl();
  ~Impl();
  UniString name;
 };


 Tag::Tag()
 {

 }

 Tag::Tag(const UniString& name)
 {

 }

 Tag::~Tag()
 {

 }

 void Tag::setName(const UniString& name)
 {

 }

};