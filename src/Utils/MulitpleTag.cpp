module;

#include <QString>
#include <QStringList>
#include <QSet>
#include <QRegularExpression>

module Utils.MultipleTag;

import std;

namespace ArtifactCore {

 class MultipleTag::Impl {
 public:
  QSet<QString> tags_;

  Impl() = default;
  Impl(const Impl& other) = default;
  Impl& operator=(const Impl& other) = default;
 };

 MultipleTag::MultipleTag() : impl_(new Impl())
 {

 }

 MultipleTag::MultipleTag(const MultipleTag& other) : impl_(new Impl(*other.impl_))
 {

 }

 MultipleTag::MultipleTag(MultipleTag&& other) noexcept : impl_(other.impl_)
 {
  other.impl_ = nullptr;
 }

 MultipleTag::~MultipleTag()
 {
  delete impl_;
 }

 MultipleTag& MultipleTag::operator=(const MultipleTag& other)
 {
  if (this != &other) {
   *impl_ = *other.impl_;
  }
  return *this;
 }

 MultipleTag& MultipleTag::operator=(MultipleTag&& other) noexcept
 {
  if (this != &other) {
   delete impl_;
   impl_ = other.impl_;
   other.impl_ = nullptr;
  }
  return *this;
 }

 void MultipleTag::addTag(const QString& tag)
 {
  if (!tag.trimmed().isEmpty()) {
   impl_->tags_.insert(tag.trimmed());
  }
 }

 void MultipleTag::addTags(const QStringList& tags)
 {
  for (const auto& tag : tags) {
   addTag(tag);
  }
 }

 void MultipleTag::removeTag(const QString& tag)
 {
  impl_->tags_.remove(tag.trimmed());
 }

 void MultipleTag::removeTags(const QStringList& tags)
 {
  for (const auto& tag : tags) {
   removeTag(tag);
  }
 }

 void MultipleTag::clear()
 {
  impl_->tags_.clear();
 }

 bool MultipleTag::hasTag(const QString& tag) const
 {
  return impl_->tags_.contains(tag.trimmed());
 }

 bool MultipleTag::hasAnyTag(const QStringList& tags) const
 {
  for (const auto& tag : tags) {
   if (hasTag(tag)) {
    return true;
   }
  }
  return false;
 }

 bool MultipleTag::hasAllTags(const QStringList& tags) const
 {
  for (const auto& tag : tags) {
   if (!hasTag(tag)) {
    return false;
   }
  }
  return true;
 }

 int MultipleTag::count() const
 {
  return impl_->tags_.size();
 }

 bool MultipleTag::isEmpty() const
 {
  return impl_->tags_.isEmpty();
 }

 QStringList MultipleTag::tags() const
 {
  return QStringList(impl_->tags_.begin(), impl_->tags_.end());
 }

 QStringList MultipleTag::sortedTags() const
 {
  QStringList result = tags();
  result.sort();
  return result;
 }

 QStringList MultipleTag::filterByPrefix(const QString& prefix) const
 {
  QStringList result;
  for (const auto& tag : impl_->tags_) {
   if (tag.startsWith(prefix, Qt::CaseInsensitive)) {
    result.append(tag);
   }
  }
  return result;
 }

 QStringList MultipleTag::filterByPattern(const QString& pattern) const
 {
  QStringList result;
  QRegularExpression regex(pattern);
  for (const auto& tag : impl_->tags_) {
   if (regex.match(tag).hasMatch()) {
    result.append(tag);
   }
  }
  return result;
 }

 MultipleTag MultipleTag::unite(const MultipleTag& other) const
 {
  MultipleTag result;
  result.impl_->tags_ = impl_->tags_.unite(other.impl_->tags_);
  return result;
 }

 MultipleTag MultipleTag::intersect(const MultipleTag& other) const
 {
  MultipleTag result;
  result.impl_->tags_ = impl_->tags_.intersect(other.impl_->tags_);
  return result;
 }

 MultipleTag MultipleTag::subtract(const MultipleTag& other) const
 {
  MultipleTag result;
  result.impl_->tags_ = impl_->tags_.subtract(other.impl_->tags_);
  return result;
 }

 QString MultipleTag::toString(const QString& separator) const
 {
  return sortedTags().join(separator);
 }

 void MultipleTag::fromString(const QString& str, const QString& separator)
 {
  clear();
  QStringList tagList = str.split(separator, Qt::SkipEmptyParts);
  addTags(tagList);
 }

 bool MultipleTag::operator==(const MultipleTag& other) const
 {
  return impl_->tags_ == other.impl_->tags_;
 }

 bool MultipleTag::operator!=(const MultipleTag& other) const
 {
  return !(*this == other);
 }

};

