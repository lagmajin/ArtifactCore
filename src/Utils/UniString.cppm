module;

#include <QString>

module Utils.String.UniString;

import std;
import Utils.Convertor.String;

namespace ArtifactCore {

 class UniString::Impl {
 private:

 public:
  Impl();
  Impl(const std::u16string& u16);
  Impl(const UniString& other);
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

 UniString::Impl::Impl(const UniString& other):str_(other.impl_->str_)
 {

 }

 UniString::Impl::Impl(const std::u16string& u16)
 {

 }
	
 UniString::Impl::~Impl()
 {

 }

 std::u16string UniString::Impl::toStdU16String() const
 {
  auto result = std::u16string();

  return result;
 }

 std::u32string UniString::Impl::toStdU32String() const
 {
  auto result = std::u32string();

  return result;
 }

 UniString::UniString() :impl_(new Impl())
 {

 }

 UniString::UniString(const UniString& other) :impl_(new Impl(other))
 {

 }

 UniString::UniString(const QString& str) :impl_(new Impl())
 {

 }

 UniString::UniString(const std::u16string& u16) :impl_(new Impl())
 {

 }

 UniString::~UniString()
 {
  delete impl_;
 }

 QString UniString::toQString() const
 {
  return impl_->str_;
 }

 void UniString::setQString(const QString& str)
 {
  impl_->str_ = str;
 }

 UniString& UniString::operator=(const UniString& other)
 {
  impl_->str_ = other.toQString();

  return *this;
 }

 UniString& UniString::operator=(UniString&& other) noexcept
 {
  impl_->str_ = other.toQString();
  return *this;
 }

 std::u16string UniString::toStdU16String() const
 {
  return impl_->toStdU16String();
 }

 std::u32string UniString::toStdU32String() const
 {
  return impl_->toStdU32String();
 }

 UniString::operator QString() const
 {
  return impl_->str_;
 }

 UniString::operator std::u16string() const
 {
  return impl_->toStdU16String();
 }

 size_t UniString::length() const
 {
  return impl_->str_.length();
 }



};