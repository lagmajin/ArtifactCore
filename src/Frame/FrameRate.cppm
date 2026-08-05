module;
class tst_QList;
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
};

FrameRate::Impl::Impl()
{
}

FrameRate::Impl::~Impl()
{
}

FrameRate::FrameRate() : impl_(new Impl)
{
}

FrameRate::FrameRate(float frameRate) : impl_(new Impl)
{
 impl_->frameRate_ = std::isfinite(frameRate) && frameRate > 0.0f ? frameRate : 30.0f;
}

FrameRate::FrameRate(const QString& str) : impl_(new Impl)
{
 impl_->frameRate_ = 30.0f;
 setFromString(str);
}

FrameRate::FrameRate(const FrameRate& frameRate) : impl_(new Impl)
{
 impl_->frameRate_ = frameRate.impl_->frameRate_;
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
   parsed = static_cast<float>(numerator / denominator);
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
 return std::abs(impl_->frameRate_ - 29.97f) < 0.02f ||
        std::abs(impl_->frameRate_ - 59.94f) < 0.02f;
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
