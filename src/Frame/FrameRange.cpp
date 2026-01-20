module;

#include <QtCore/QString>
#include <QtCore/QJsonObject>
#include <algorithm>
#include <limits>

module Frame.Range;

import std;
import Frame.Position;
import Frame.Offset;
import Frame.Rate;

namespace ArtifactCore {

 class FrameRange::Impl {
 public:
  int64_t start_ = 0;
  int64_t end_ = 0;

  Impl() = default;
  Impl(int64_t start, int64_t end) : start_(start), end_(end) {}
  
  bool isValid() const {
   return start_ <= end_;
  }
  
  int64_t duration() const {
   return end_ - start_;
  }
 };

 // �R���X�g���N�^

 FrameRange::FrameRange() : impl_(new Impl()) {}

 FrameRange::FrameRange(int64_t start, int64_t end) 
  : impl_(new Impl(start, end)) {}

 FrameRange::FrameRange(const FramePosition& start, const FramePosition& end)
  : impl_(new Impl(start.framePosition(), end.framePosition())) {}

 FrameRange::FrameRange(const FrameRange& other)
  : impl_(new Impl(*other.impl_)) {}

 FrameRange::FrameRange(FrameRange&& other) noexcept
  : impl_(other.impl_) {
  other.impl_ = nullptr;
 }

 FrameRange::~FrameRange() {
  delete impl_;
 }

 // ������Z�q

 FrameRange& FrameRange::operator=(const FrameRange& other) {
  if (this != &other) {
   *impl_ = *other.impl_;
  }
  return *this;
 }

 FrameRange& FrameRange::operator=(FrameRange&& other) noexcept {
  if (this != &other) {
   delete impl_;
   impl_ = other.impl_;
   other.impl_ = nullptr;
  }
  return *this;
 }

 // �͈͐ݒ�

 void FrameRange::setStart(int64_t start) {
  impl_->start_ = start;
 }

 void FrameRange::setEnd(int64_t end) {
  impl_->end_ = end;
 }

 void FrameRange::setRange(int64_t start, int64_t end) {
  impl_->start_ = start;
  impl_->end_ = end;
 }

 void FrameRange::setDuration(int64_t duration) {
  impl_->end_ = impl_->start_ + duration;
 }

 // �͈͎擾

 int64_t FrameRange::start() const {
  return impl_->start_;
 }

 int64_t FrameRange::end() const {
  return impl_->end_;
 }

 int64_t FrameRange::duration() const {
  return impl_->duration();
 }

 int64_t FrameRange::length() const {
  return impl_->duration();
 }

 FramePosition FrameRange::startPosition() const {
  return FramePosition(impl_->start_);
 }

 FramePosition FrameRange::endPosition() const {
  return FramePosition(impl_->end_);
 }

 // ����

 bool FrameRange::isValid() const {
  return impl_->isValid();
 }

 bool FrameRange::isEmpty() const {
  return impl_->start_ == impl_->end_;
 }

 bool FrameRange::isInfinite() const {
  return impl_->start_ == std::numeric_limits<int64_t>::min() &&
         impl_->end_ == std::numeric_limits<int64_t>::max();
 }

 // �͈̓`�F�b�N

 bool FrameRange::contains(int64_t frame) const {
  return frame >= impl_->start_ && frame <= impl_->end_;
 }

 bool FrameRange::contains(const FramePosition& position) const {
  return contains(position.framePosition());
 }

 bool FrameRange::contains(const FrameRange& other) const {
  return other.start() >= impl_->start_ && other.end() <= impl_->end_;
 }

 bool FrameRange::overlaps(const FrameRange& other) const {
  return !(impl_->end_ < other.start() || impl_->start_ > other.end());
 }

 bool FrameRange::touches(const FrameRange& other) const {
  return overlaps(other) || 
         impl_->end_ + 1 == other.start() || 
         other.end() + 1 == impl_->start_;
 }

 // �͈͑���

 void FrameRange::expand(int64_t frames) {
  impl_->start_ -= frames;
  impl_->end_ += frames;
 }

 void FrameRange::expandStart(int64_t frames) {
  impl_->start_ -= frames;
 }

 void FrameRange::expandEnd(int64_t frames) {
  impl_->end_ += frames;
 }

 void FrameRange::shrink(int64_t frames) {
  impl_->start_ += frames;
  impl_->end_ -= frames;
 }

 void FrameRange::shrinkStart(int64_t frames) {
  impl_->start_ += frames;
 }

 void FrameRange::shrinkEnd(int64_t frames) {
  impl_->end_ -= frames;
 }

 void FrameRange::shift(int64_t frames) {
  impl_->start_ += frames;
  impl_->end_ += frames;
 }

 void FrameRange::shift(const FrameOffset& offset) {
  shift(offset.value());
 }

 FrameRange FrameRange::shifted(int64_t frames) const {
  return FrameRange(impl_->start_ + frames, impl_->end_ + frames);
 }

 FrameRange FrameRange::expanded(int64_t frames) const {
  return FrameRange(impl_->start_ - frames, impl_->end_ + frames);
 }

 FrameRange FrameRange::shrinked(int64_t frames) const {
  return FrameRange(impl_->start_ + frames, impl_->end_ - frames);
 }

 // �͈͉��Z

 FrameRange FrameRange::united(const FrameRange& other) const {
  return FrameRange(
   std::min(impl_->start_, other.start()),
   std::max(impl_->end_, other.end())
  );
 }

 FrameRange FrameRange::intersected(const FrameRange& other) const {
  int64_t newStart = std::max(impl_->start_, other.start());
  int64_t newEnd = std::min(impl_->end_, other.end());
  
  if (newStart > newEnd) {
   return FrameRange::invalid();
  }
  
  return FrameRange(newStart, newEnd);
 }

 bool FrameRange::intersects(const FrameRange& other, FrameRange& result) const {
  result = intersected(other);
  return result.isValid();
 }

 // �N���b�s���O

 void FrameRange::clip(const FrameRange& bounds) {
  impl_->start_ = std::max(impl_->start_, bounds.start());
  impl_->end_ = std::min(impl_->end_, bounds.end());
 }

 FrameRange FrameRange::clipped(const FrameRange& bounds) const {
  return FrameRange(
   std::max(impl_->start_, bounds.start()),
   std::min(impl_->end_, bounds.end())
  );
 }

 int64_t FrameRange::clampFrame(int64_t frame) const {
  return std::clamp(frame, impl_->start_, impl_->end_);
 }

 FramePosition FrameRange::clampPosition(const FramePosition& position) const {
  return FramePosition(clampFrame(position.framePosition()));
 }

 // �C�e���[�V����

 FrameRange::Iterator FrameRange::beginIterator() const {
  return Iterator(impl_->start_, impl_->end_);
 }

 FrameRange::Iterator FrameRange::endIterator() const {
  return Iterator(impl_->end_ + 1, impl_->end_);
 }

 // ���[�e�B���e�B

 FrameRange FrameRange::normalized() const {
  if (impl_->start_ <= impl_->end_) {
   return *this;
  }
  return FrameRange(impl_->end_, impl_->start_);
 }

 void FrameRange::normalize() {
  if (impl_->start_ > impl_->end_) {
   std::swap(impl_->start_, impl_->end_);
  }
 }

 std::vector<int64_t> FrameRange::frames() const {
  std::vector<int64_t> result;
  int64_t count = duration() + 1;
  
  if (count <= 0 || count > 1000000) {  // ���S�̂��ߏ��
   return result;
  }
  
  result.reserve(static_cast<size_t>(count));
  for (int64_t i = impl_->start_; i <= impl_->end_; ++i) {
   result.push_back(i);
  }
  
  return result;
 }

 std::vector<int64_t> FrameRange::uniformSample(int count) const {
  std::vector<int64_t> result;
  
  if (count <= 0 || !isValid()) {
   return result;
  }
  
  result.reserve(count);
  int64_t dur = duration();
  
  if (dur == 0) {
   result.push_back(impl_->start_);
   return result;
  }
  
  for (int i = 0; i < count; ++i) {
   int64_t frame = impl_->start_ + (dur * i) / count;
   result.push_back(frame);
  }
  
  return result;
 }

 // ���ԕϊ�

 double FrameRange::durationSeconds(double fps) const {
  if (fps <= 0.0) return 0.0;
  return duration() / fps;
 }

 double FrameRange::durationSeconds(const FrameRate& rate) const {
  return durationSeconds(rate.framerate());
 }

 QString FrameRange::toTimecode(double fps) const {
  if (fps <= 0.0) return QString();
  
  auto startSec = impl_->start_ / fps;
  auto endSec = impl_->end_ / fps;
  
  auto formatTime = [fps](double seconds) {
   int hours = static_cast<int>(seconds / 3600);
   int minutes = static_cast<int>((seconds - hours * 3600) / 60);
   int secs = static_cast<int>(seconds - hours * 3600 - minutes * 60);
   int frames = static_cast<int>((seconds - static_cast<int>(seconds)) * fps);
   
   return QString("%1:%2:%3:%4")
    .arg(hours, 2, 10, QChar('0'))
    .arg(minutes, 2, 10, QChar('0'))
    .arg(secs, 2, 10, QChar('0'))
    .arg(frames, 2, 10, QChar('0'));
  };
  
  return formatTime(startSec) + " - " + formatTime(endSec);
 }

 QString FrameRange::toTimecode(const FrameRate& rate) const {
  return toTimecode(rate.framerate());
 }

 // �V���A���C�Y

 QJsonObject FrameRange::toJson() const {
  QJsonObject obj;
  obj["start"] = static_cast<qint64>(impl_->start_);
  obj["end"] = static_cast<qint64>(impl_->end_);
  return obj;
 }

 FrameRange FrameRange::fromJson(const QJsonObject& json) {
  int64_t start = json["start"].toInteger();
  int64_t end = json["end"].toInteger();
  return FrameRange(start, end);
 }

 QString FrameRange::toString() const {
  return QString("[%1, %2]").arg(impl_->start_).arg(impl_->end_);
 }

 FrameRange FrameRange::fromString(const QString& str) {
  QString trimmed = str.trimmed();
  if (trimmed.startsWith('[') && trimmed.endsWith(']')) {
   trimmed = trimmed.mid(1, trimmed.length() - 2);
  }
  
  QStringList parts = trimmed.split(',');
  if (parts.size() != 2) {
   return FrameRange::invalid();
  }
  
  bool ok1, ok2;
  int64_t start = parts[0].trimmed().toLongLong(&ok1);
  int64_t end = parts[1].trimmed().toLongLong(&ok2);
  
  if (!ok1 || !ok2) {
   return FrameRange::invalid();
  }
  
  return FrameRange(start, end);
 }

 // ��r���Z�q

 bool FrameRange::operator==(const FrameRange& other) const {
  return impl_->start_ == other.start() && impl_->end_ == other.end();
 }

 bool FrameRange::operator!=(const FrameRange& other) const {
  return !(*this == other);
 }

 bool FrameRange::operator<(const FrameRange& other) const {
  if (impl_->start_ != other.start()) {
   return impl_->start_ < other.start();
  }
  return impl_->end_ < other.end();
 }

 bool FrameRange::operator<=(const FrameRange& other) const {
  return *this < other || *this == other;
 }

 bool FrameRange::operator>(const FrameRange& other) const {
  return !(*this <= other);
 }

 bool FrameRange::operator>=(const FrameRange& other) const {
  return !(*this < other);
 }

 // ����Ȕ͈�

 FrameRange FrameRange::invalid() {
  return FrameRange(1, 0);  // start > end �Ŗ���
 }

 FrameRange FrameRange::infinite() {
  return FrameRange(
   std::numeric_limits<int64_t>::min(),
   std::numeric_limits<int64_t>::max()
  );
 }

 FrameRange FrameRange::zero() {
  return FrameRange(0, 0);
 }

 FrameRange FrameRange::fromDuration(int64_t start, int64_t duration) {
  return FrameRange(start, start + duration);
 }

}
