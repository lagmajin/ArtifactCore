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

 Tag::Impl::Impl()
 {

 }

 Tag::Impl::~Impl()
 {

 }

 Tag::Tag() :impl_(new Impl())
 {

 }

 Tag::Tag(const UniString& name) :impl_(new Impl())
 {

 }

 Tag::Tag(const Tag& other) :impl_(new Impl())
 {

 }

 Tag::Tag(Tag&& other) noexcept :impl_(new Impl())
 {

 }

 Tag::~Tag()
 {
  delete impl_;
 }
 

 void Tag::setName(const UniString& name)
 {

 }

Tag& Tag::operator=(const Tag& other)
 {

 return *this;
 }

Tag& Tag::operator=(Tag&& other) noexcept
 {
 return *this;
 }

};