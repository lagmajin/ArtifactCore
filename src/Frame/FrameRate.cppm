module;
#include <QList>
#include <QJsonObject>
#include <QRegularExpression>
#include <QString>
#include <algorithm>
#include <cmath>
#include <utility>
module Frame.Rate;

import Serialization.JsonAdapter;
import Serialization.SchemaMigration;

class tst_QList;

namespace {
const bool kFrameRateSerializationRegistered = [] {
    ArtifactCore::Serialization::registerJsonSerializableType<ArtifactCore::FrameRate>(
        QStringLiteral("FrameRate"), 1);
    ArtifactCore::Serialization::SchemaMigrationRegistry::instance().registerMigration(
        QStringLiteral("FrameRate"), 0, 1,
        [](const QJsonObject& object) { return object; });
    return true;
}();
}

namespace ArtifactCore {

class FrameRate::Impl {
private:
public:
 Impl();
 ~Impl();
 float frameRate_ = 0.0f;
 std::int64_t numerator_ = 1;
 std::int64_t denominator_ = 1;
 bool rationalExact_ = false;
};

FrameRate::Impl::Impl()
{
}

FrameRate::Impl::~Impl()
{
}

FrameRate FrameRate::fromRational(std::int64_t numerator, std::int64_t denominator)
{
    FrameRate rate;
    rate.setRationalRate(numerator, denominator);
    return rate;
}

void FrameRate::setRationalRate(std::int64_t numerator, std::int64_t denominator)
{
    if (numerator <= 0 || denominator <= 0) {
        return;
    }
    impl_->numerator_ = numerator;
    impl_->denominator_ = denominator;
    impl_->rationalExact_ = true;
    impl_->frameRate_ = static_cast<float>(
        static_cast<double>(numerator) / static_cast<double>(denominator));
}

std::int64_t FrameRate::numerator() const
{
    return impl_->numerator_;
}

std::int64_t FrameRate::denominator() const
{
    return impl_->denominator_;
}

bool FrameRate::hasExactRational() const
{
    return impl_->rationalExact_;
}

double FrameRate::exactFps() const
{
    if (impl_->rationalExact_) {
        return static_cast<double>(impl_->numerator_) /
               static_cast<double>(impl_->denominator_);
    }
    return static_cast<double>(impl_->frameRate_);
}

FrameRate::FrameRate() : impl_(new Impl)
{
}

FrameRate::FrameRate(float frameRate) : impl_(new Impl)
{
 setFrameRate(frameRate);
}

FrameRate::FrameRate(const QString& str) : impl_(new Impl)
{
 impl_->frameRate_ = 30.0f;
 setFromString(str);
}

FrameRate::FrameRate(const FrameRate& frameRate) : impl_(new Impl)
{
 impl_->frameRate_ = frameRate.impl_->frameRate_;
 impl_->numerator_ = frameRate.impl_->numerator_;
 impl_->denominator_ = frameRate.impl_->denominator_;
 impl_->rationalExact_ = frameRate.impl_->rationalExact_;
}

FrameRate::FrameRate(FrameRate&& framerate) noexcept : impl_(framerate.impl_)
{
 framerate.impl_ = nullptr;
}

FrameRate::~FrameRate()
{
 delete impl_;
}

void FrameRate::setFrameRate(float frame)
{
 impl_->frameRate_ = std::isfinite(frame) && frame > 0.0f ? frame : 30.0f;
 // A plain float assignment no longer carries an exact rational rate.
 impl_->numerator_ = 1;
 impl_->denominator_ = 1;
 impl_->rationalExact_ = false;
}

void FrameRate::speedUp(float frame)
{
 if (std::isfinite(frame)) setFrameRate(impl_->frameRate_ + std::max(0.0f, frame));
}

void FrameRate::speedDown(float frame)
{
 if (std::isfinite(frame)) setFrameRate(impl_->frameRate_ - std::max(0.0f, frame));
}

void FrameRate::setFromJson(const QJsonObject& object)
{
 if (object.contains(QStringLiteral("numerator")) &&
     object.contains(QStringLiteral("denominator"))) {
  const std::int64_t numerator = static_cast<std::int64_t>(
      object.value(QStringLiteral("numerator")).toDouble(0.0));
  const std::int64_t denominator = static_cast<std::int64_t>(
      object.value(QStringLiteral("denominator")).toDouble(0.0));
  setRationalRate(numerator, denominator);
  return;
 }
 if (object.contains(QStringLiteral("frameRate"))) {
  setFrameRate(static_cast<float>(object.value(QStringLiteral("frameRate")).toDouble(30.0)));
 } else if (object.contains(QStringLiteral("fps"))) {
  setFrameRate(static_cast<float>(object.value(QStringLiteral("fps")).toDouble(30.0)));
 } else if (object.contains(QStringLiteral("value"))) {
  setFromString(object.value(QStringLiteral("value")).toString());
 }
}

void FrameRate::readFromJson(QJsonObject& object) const
{
 writeToJson(object);
}

void FrameRate::writeToJson(QJsonObject& object) const
{
 object[QStringLiteral("frameRate")] = static_cast<double>(impl_->frameRate_);
 object[QStringLiteral("dropFrame")] = hasDropframe();
 if (impl_->rationalExact_) {
  object[QStringLiteral("numerator")] = static_cast<double>(impl_->numerator_);
  object[QStringLiteral("denominator")] = static_cast<double>(impl_->denominator_);
 }
}

void FrameRate::setFromString(const QString& value)
{
 QString text = value.trimmed();
 text.remove(QRegularExpression(QStringLiteral("(?i)fps")));
 text.remove(QRegularExpression(QStringLiteral("(?i)df")));
 bool ok = false;
 float parsed = 0.0f;
 const QStringList fraction = text.trimmed().split(QLatin1Char('/'));
 if (fraction.size() == 2) {
  bool numeratorOk = false;
  bool denominatorOk = false;
  const double numerator = fraction[0].trimmed().toDouble(&numeratorOk);
  const double denominator = fraction[1].trimmed().toDouble(&denominatorOk);
  if (numeratorOk && denominatorOk && std::abs(denominator) > 1e-12) {
   // Keep integer fractions exact instead of collapsing to float.
   setRationalRate(static_cast<std::int64_t>(numerator),
                   static_cast<std::int64_t>(denominator));
   ok = true;
  }
 } else {
  parsed = text.trimmed().toFloat(&ok);
 }
 if (ok && std::isfinite(parsed) && parsed > 0.0f) setFrameRate(parsed);
}

FrameRate& FrameRate::operator=(float rate)
{
 setFrameRate(rate);
 return *this;
}

FrameRate& FrameRate::operator=(const QString& str)
{
 setFromString(str);
 return *this;
}

FrameRate& FrameRate::operator=(const FrameRate& framerate)
{
 if (this != &framerate) {
  impl_->frameRate_ = framerate.impl_->frameRate_;
  impl_->numerator_ = framerate.impl_->numerator_;
  impl_->denominator_ = framerate.impl_->denominator_;
  impl_->rationalExact_ = framerate.impl_->rationalExact_;
 }
 return *this;
}

FrameRate& FrameRate::operator=(FrameRate&& framerate) noexcept
{
 if (this != &framerate) {
  delete impl_;
  impl_ = framerate.impl_;
  framerate.impl_ = nullptr;
 }
 return *this;
}

UniString FrameRate::toString() const
{
 return UniString(QString::number(impl_->frameRate_, 'f', 6).trimmed());
}

float FrameRate::framerate() const
{
 return impl_->frameRate_;
}

UniString FrameRate::toDisplayString(bool includeDropframe) const
{
 const bool drop = includeDropframe && hasDropframe();
 const QString suffix = drop ? QStringLiteral(" DF") : QStringLiteral(" fps");
 return UniString(QString::number(impl_->frameRate_, 'f', drop ? 2 : 3) + suffix);
}

bool FrameRate::hasDropframe() const
{
 if (impl_->rationalExact_) {
  // Classic SMPTE drop-frame rates: 23.976 / 29.97 / 47.952 / 59.94.
  return impl_->denominator_ == 1001 &&
         (impl_->numerator_ == 24000 || impl_->numerator_ == 30000 ||
          impl_->numerator_ == 48000 || impl_->numerator_ == 60000);
 }
 return std::abs(impl_->frameRate_ - 29.97f) < 0.02f ||
        std::abs(impl_->frameRate_ - 59.94f) < 0.02f ||
        std::abs(impl_->frameRate_ - 23.976f) < 0.02f ||
        std::abs(impl_->frameRate_ - 47.952f) < 0.02f;
}

void FrameRate::swap(FrameRate& other) noexcept
{
 std::swap(impl_, other.impl_);
}

QJsonObject FrameRate::toJson() const
{
 QJsonObject object;
 writeToJson(object);
 return object;
}

FrameRate FrameRate::fromJsonStatic(const QJsonObject& object)
{
 FrameRate result;
 result.setFromJson(object);
 return result;
}

bool operator==(const FrameRate& framerate1, const FrameRate& framerate2)
{
 return framerate1.framerate() == framerate2.framerate();
}

bool operator!=(const FrameRate& framerate1, const FrameRate& framerate2)
{
 return !(framerate1 == framerate2);
}

FrameRateOffset::FrameRateOffset(std::int64_t value) : value_(value) {}

bool operator==(const FrameRateOffset& lhs, const FrameRateOffset& rhs)
{
 return lhs.value() == rhs.value();
}

bool operator!=(const FrameRateOffset& lhs, const FrameRateOffset& rhs)
{
 return !(lhs == rhs);
}

bool operator<=(const FrameRateOffset& lhs, const FrameRateOffset& rhs)
{
 return lhs.value() <= rhs.value();
}

}
