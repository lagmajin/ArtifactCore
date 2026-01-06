module;
#include <QString>
#include <QStringList>
module Time.Code;

import std;

namespace ArtifactCore {

 class TimeCode::Impl {
 private:

 public:
  Impl();
  ~Impl();
  int totalFrames = 0;
  double fps = 30.0;
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
  totalFrames = ((h * 3600 + m * 60 + s) * (int)fps) + f;
 }

 TimeCode::TimeCode() :impl_(new Impl())
 {

 }

 TimeCode::TimeCode(int frame, double fps) :impl_(new Impl())
 {

 }

 TimeCode::TimeCode(int h, int m, int s, int f, double fps) :impl_(new Impl())
 {
  impl_->fps = fps;
  //impl_->fromHMSF(h, m, s, f);
 }

 TimeCode::TimeCode(TimeCode&& other) noexcept : impl_(std::move(other.impl_))
 {

 }

 TimeCode::~TimeCode()
 {
  delete impl_;
 }

 void TimeCode::setByFrame(int frame)
 {

 }

 void TimeCode::setByHMSF(int h, int m, int s, int f)
 {

 }

 void TimeCode::toHMSF(int& h, int& m, int& s, int& f) const
 {
  double fps = impl_->fps;
  int total_frames = impl_->totalFrames;

  // 非ドロップフレームを前提とした簡易計算
  // ※29.97fps等のドロップフレーム対応が必要な場合は別途ロジックが必要
  h = total_frames / (int)(3600 * fps);
  m = (total_frames / (int)(60 * fps)) % 60;
  s = (total_frames / (int)fps) % 60;
  f = total_frames % (int)fps;
 }

 std::string TimeCode::toStdString() const
 {

  return std::string();
 }

 TimeCode& TimeCode::operator=(TimeCode&&) noexcept
 {
  return *this;
 }

 QString TimeCode::toString() const
 {
  return QString();
 }

 void TimeCode::setFromQString(const QString& str)
 {
  // 1. "." を ":" に置換（1.10.0 -> 1:10:0）
  QString cleanStr = str;
  if (cleanStr.endsWith('.')) {
   cleanStr += "00"; // "10." -> "1000" (10秒00フレーム)
  }
  cleanStr.replace('.', ':');

  // 2. セパレータで分割
  QStringList parts = cleanStr.split(':');

  int h = 0, m = 0, s = 0, f = 0;

  // 右側から（フレーム側から）埋めていくAE風ロジック
  int count = parts.size();
  if (count > 0) f = parts.at(count - 1).toInt();
  if (count > 1) s = parts.at(count - 2).toInt();
  if (count > 2) m = parts.at(count - 3).toInt();
  if (count > 3) h = parts.at(count - 4).toInt();

  setByHMSF(h, m, s, f);
 }

 double TimeCode::fps() const
 {
  return impl_->fps;
 }

 int TimeCode::frame() const
 {
  return impl_->totalFrames;
 }

};