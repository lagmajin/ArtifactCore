module;

#include <QString>

module Utils.UniString;

import std;
import Utils.Convertor.String;

namespace ArtifactCore {

 class UniString::Impl{
 private:
  
 public:
  Impl(); 
  Impl(const Impl& other);
  ~Impl();
  QString str_;
  std::u16string toStdU16String() const;
  std::u32string toStdU32String() const;
 };

 UniString::Impl::Impl()
 {

 }

 UniString::Impl::Impl(const Impl& other) : str_(other.str_)
 {

 }

 UniString::Impl::~Impl()
 {

 }

 std::u16string UniString::Impl::toStdU16String() const
 {
  return std::u16string();
 }

 UniString::UniString() :impl_(new Impl())
 {

 }

 UniString::UniString(const UniString& other) :impl_(new Impl())
 {

 }

 UniString::~UniString()
 {
  delete impl_;
 }

 QString UniString::toQString() const
 {
  return QString();
 }

 void UniString::setQString(const QString& str)
 {

 }

 UniString& UniString::operator=(const UniString& other)
 {
  return *this;
 }

 UniString& UniString::operator=(UniString&& other) noexcept
 {
  return *this;
 }

 std::u16string UniString::toStdU16String() const
 {
  return impl_->toStdU16String();
 }

 std::u32string UniString::toStdU32String() const
 {
  return std::u32string();
 }

 UniString::operator QString() const
 {
  return impl_->str_;
 }

 UniString::operator std::u16string() const
 {
  return std::u16string();
 }

 size_t UniString::length() const
 {
  return impl_->str_.length();
 }



};