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

 UniString::UniString(const std::string& str) :impl_(new Impl())
 {
  // Convert std::string to QString
  impl_->str_ = QString::fromStdString(str);
 }

 UniString::UniString(const char* str) :impl_(new Impl())
 {
  // Convert const char* to QString (assuming UTF-8)
  impl_->str_ = QString::fromUtf8(str);
 }

 UniString::UniString(const char16_t* str) :impl_(new Impl())
 {
  // Convert const char16_t* to QString
  impl_->str_ = QString::fromUtf16(str);
 }

 UniString::UniString(const char32_t* str) :impl_(new Impl())
 {
  // Convert const char32_t* to QString (UCS-4)
  impl_->str_ = QString::fromUcs4(str);
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

 UniString::operator std::string() const
 {
  return std::string(toStdU16String().begin(), toStdU16String().end());
 }

 size_t UniString::length() const
 {
  return impl_->str_.length();
 }

 // Comparison operators implementation
 bool UniString::operator==(const UniString& other) const
 {
  return impl_->str_ == other.impl_->str_;
 }

 bool UniString::operator!=(const UniString& other) const
 {
  return impl_->str_ != other.impl_->str_;
 }

 bool UniString::operator<(const UniString& other) const
 {
  return impl_->str_ < other.impl_->str_;
 }

 bool UniString::operator<=(const UniString& other) const
 {
  return impl_->str_ <= other.impl_->str_;
 }

 bool UniString::operator>(const UniString& other) const
 {
  return impl_->str_ > other.impl_->str_;
 }

 bool UniString::operator>=(const UniString& other) const
 {
  return impl_->str_ >= other.impl_->str_;
 }

};