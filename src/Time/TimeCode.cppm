module;
#include <QString>
#include <QStringList>
#include <cstdio>
#include <cmath>
#include <string>
module Time.Code;

import Time.Rational;

namespace ArtifactCore {

class TimeCode::Impl {
private:
public:
 Impl();
 ~Impl();
 int totalFrames = 0;
 double fps = 30.0;
 bool dropFrame = false;
 void fromHMSF(int h, int m, int s, int f);
};

TimeCode::Impl::Impl()
{
}

TimeCode::Impl::~Impl()
{
}

void TimeCode::Impl::fromHMSF(int h, int m, int s, int f)
{
 // Timecode labels use the nominal whole-frame rate (29.97 -> 30,
 // 59.94 -> 60). Truncating the rate turns 29.97 into 29 and causes the
 // displayed timecode to drift against the actual frame timeline.
 const int nominalFps = std::max(1, static_cast<int>(std::lround(fps)));
 const int dropCount = dropFrame && (nominalFps == 30 || nominalFps == 60)
     ? nominalFps / 15
     : 0;
 const int nominalFrame = ((h * 3600 + m * 60 + s) * nominalFps) + f;
 const int totalMinutes = h * 60 + m;
 const int droppedFrames = dropCount * (totalMinutes - totalMinutes / 10);
 totalFrames = nominalFrame - droppedFrames;
}

TimeCode::TimeCode() : impl_(new Impl())
{
}

TimeCode::TimeCode(int frame, double fps) : impl_(new Impl())
{
 impl_->totalFrames = frame;
 impl_->fps = fps > 0.0 ? fps : 30.0;
}

TimeCode::TimeCode(int h, int m, int s, int f, double fps) : impl_(new Impl())
{
 impl_->fps = fps > 0.0 ? fps : 30.0;
 impl_->fromHMSF(h, m, s, f);
}

TimeCode::TimeCode(const TimeCode& other) : impl_(new Impl())
{
 impl_->totalFrames = other.impl_ ? other.impl_->totalFrames : 0;
 impl_->fps = other.impl_ ? other.impl_->fps : 30.0;
 impl_->dropFrame = other.impl_ && other.impl_->dropFrame;
}

TimeCode& TimeCode::operator=(const TimeCode& other)
{
 if (this != &other) {
  if (!impl_) {
   impl_ = new Impl();
  }
  impl_->totalFrames = other.impl_ ? other.impl_->totalFrames : 0;
  impl_->fps = other.impl_ ? other.impl_->fps : 30.0;
  impl_->dropFrame = other.impl_ && other.impl_->dropFrame;
 }
 return *this;
}

void TimeCode::setDropFrame(bool enabled)
{
 impl_->dropFrame = enabled &&
     (std::lround(impl_->fps) == 30 || std::lround(impl_->fps) == 60);
}

bool TimeCode::isDropFrame() const
{
 return impl_->dropFrame;
}

TimeCode::TimeCode(TimeCode&& other) noexcept : impl_(other.impl_)
{
 other.impl_ = nullptr;
}

TimeCode::~TimeCode()
{
 if (impl_) {
  delete impl_;
  impl_ = nullptr;
 }
}

void TimeCode::setByFrame(int frame)
{
 impl_->totalFrames = frame;
}

void TimeCode::setByHMSF(int h, int m, int s, int f)
{
 impl_->fromHMSF(h, m, s, f);
}

void TimeCode::toHMSF(int& h, int& m, int& s, int& f) const
{
 const double fps = impl_->fps;
 const int total_frames = impl_->totalFrames;
 const int nominalFps = std::max(1, static_cast<int>(std::lround(fps)));
 if (!impl_->dropFrame || (nominalFps != 30 && nominalFps != 60)) {
  h = total_frames / (3600 * nominalFps);
  m = (total_frames / (60 * nominalFps)) % 60;
  s = (total_frames / nominalFps) % 60;
  f = total_frames % nominalFps;
  return;
 }

 const int dropCount = nominalFps / 15;
 const int framesPerMinute = nominalFps * 60;
 const int framesPerTenMinutes = framesPerMinute * 10 - dropCount * 9;
 const int nonNegativeFrames = std::max(0, total_frames);
 const int tenMinuteBlocks = nonNegativeFrames / framesPerTenMinutes;
 const int remainder = nonNegativeFrames % framesPerTenMinutes;
 int nominalFrame = nonNegativeFrames + dropCount * 9 * tenMinuteBlocks;
 if (remainder >= dropCount) {
  nominalFrame += dropCount *
      ((remainder - dropCount) / (framesPerMinute - dropCount) + 1);
 }
 h = nominalFrame / (3600 * nominalFps);
 m = (nominalFrame / (60 * nominalFps)) % 60;
 s = (nominalFrame / nominalFps) % 60;
 f = nominalFrame % nominalFps;
}

std::string TimeCode::toStdString() const
{
 int h, m, s, f;
 toHMSF(h, m, s, f);
 const char frameSeparator = impl_->dropFrame ? ';' : ':';
 char buf[16];
 std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d%c%02d", h, m, s,
               frameSeparator, f);
 return std::string(buf);
}

TimeCode& TimeCode::operator=(TimeCode&& other) noexcept
{
 if (this != &other) {
  delete impl_;
  impl_ = other.impl_;
  other.impl_ = nullptr;
 }
 return *this;
}

QString TimeCode::toString() const
{
 int h, m, s, f;
 toHMSF(h, m, s, f);
 // SMPTE drop-frame notation marks the seconds/frames boundary with ';'.
 const char frameSeparator = impl_->dropFrame ? ';' : ':';
 return QString("%1:%2:%3%c%4")
  .arg(h, 2, 10, QChar('0'))
  .arg(m, 2, 10, QChar('0'))
  .arg(s, 2, 10, QChar('0'))
  .arg(frameSeparator)
  .arg(f, 2, 10, QChar('0'));
}

void TimeCode::setFromQString(const QString& str)
{
 QString cleanStr = str;
 if (cleanStr.endsWith('.')) {
  cleanStr += "00";
 }
 cleanStr.replace('.', ':');
 // Accept SMPTE drop-frame strings ("HH:MM:SS;FF") as well.
 cleanStr.replace(';', ':');

 const QStringList parts = cleanStr.split(':');
 int h = 0, m = 0, s = 0, f = 0;
 const int count = parts.size();
 if (count > 0) f = parts.at(count - 1).toInt();
 if (count > 1) s = parts.at(count - 2).toInt();
 if (count > 2) m = parts.at(count - 3).toInt();
 if (count > 3) h = parts.at(count - 4).toInt();

 setByHMSF(h, m, s, f);
}

double TimeCode::fps() const { return impl_->fps; }
int TimeCode::frame() const { return impl_->totalFrames; }

double TimeCode::toSeconds() const
{
 if (impl_->fps <= 0.0) return 0.0;
 return static_cast<double>(impl_->totalFrames) / impl_->fps;
}

RationalTime TimeCode::toRationalTime() const
{
 int64_t scale = static_cast<int64_t>(std::lround(impl_->fps));
 if (scale <= 0) scale = 30;
 return RationalTime::fromFrameCount(static_cast<int64_t>(impl_->totalFrames), scale);
}

TimeCode TimeCode::fromRationalTime(const RationalTime& rt, double fps)
{
 if (fps <= 0.0) fps = 30.0;
 const int64_t frameCount = rt.toFrameCount(static_cast<int64_t>(std::lround(fps)));
 return TimeCode(static_cast<int>(frameCount), fps);
}

void TimeCode::setFromRationalTime(const RationalTime& rt)
{
 int64_t scale = static_cast<int64_t>(std::lround(impl_->fps));
 if (scale <= 0) scale = 30;
 impl_->totalFrames = static_cast<int>(rt.toFrameCount(scale));
}

}
