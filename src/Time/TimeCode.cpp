module;
#include <QString>
#include <QStringList>
module Time.Code;

import std;
import Time.Rational;

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
  int h, m, s, f;
  toHMSF(h, m, s, f);
  // HH:MM:SS:FF 形式
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d:%02d", h, m, s, f);
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
  return QString("%1:%2:%3:%4")
   .arg(h, 2, 10, QChar('0'))
   .arg(m, 2, 10, QChar('0'))
   .arg(s, 2, 10, QChar('0'))
   .arg(f, 2, 10, QChar('0'));
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

 double TimeCode::toSeconds() const
 {
  if (impl_->fps <= 0.0) return 0.0;
  return static_cast<double>(impl_->totalFrames) / impl_->fps;
 }

 RationalTime TimeCode::toRationalTime() const
 {
  // fpsをscaleとして使用し、フレーム単位の精度を保持
  // 例: 30fpsで10フレーム -> RationalTime(10, 30) = 1/3秒
  int64_t scale = static_cast<int64_t>(impl_->fps);
  if (scale <= 0) scale = 30; // フォールバック
  return RationalTime(static_cast<int64_t>(impl_->totalFrames), scale);
 }

 TimeCode TimeCode::fromRationalTime(const RationalTime& rt, double fps)
 {
  if (fps <= 0.0) fps = 30.0;
  // RationalTimeを指定のfpsにリスケールしてフレーム数を取得
  int64_t frameCount = rt.rescaledTo(static_cast<int64_t>(fps));
  return TimeCode(static_cast<int>(frameCount), fps);
 }

 void TimeCode::setFromRationalTime(const RationalTime& rt)
 {
  // 現在のfpsを保持したままフレーム数を更新
  int64_t scale = static_cast<int64_t>(impl_->fps);
  if (scale <= 0) scale = 30;
  impl_->totalFrames = static_cast<int>(rt.rescaledTo(scale));
 }

};