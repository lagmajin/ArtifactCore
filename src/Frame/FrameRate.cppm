module;
#include <utility>
#include <QList>
module Frame.Rate;

//import std;

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
 impl_->frameRate_ = frameRate;
}

FrameRate::FrameRate(const QString& str) : impl_(new Impl)
{
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
 impl_->frameRate_ = frame;
}

void FrameRate::speedUp(float)
{
}

void FrameRate::speedDown(float)
{
}

void FrameRate::setFromJson(const QJsonObject&)
{
}

void FrameRate::readFromJson(QJsonObject&) const
{
}

void FrameRate::writeToJson(QJsonObject&) const
{
}

void FrameRate::setFromString(const QString&)
{
}

FrameRate& FrameRate::operator=(float rate)
{
 impl_->frameRate_ = rate;
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
 return UniString();
}

float FrameRate::framerate() const
{
 return impl_->frameRate_;
}

UniString FrameRate::toDisplayString(bool) const
{
 return UniString();
}

bool FrameRate::hasDropframe() const
{
 return false;
}

void FrameRate::swap(FrameRate&) noexcept
{
}

bool operator==(const FrameRate& framerate1, const FrameRate& framerate2)
{
 return framerate1.framerate() == framerate2.framerate();
}

bool operator!=(const FrameRate& framerate1, const FrameRate& framerate2)
{
 return !(framerate1 == framerate2);
}

}
