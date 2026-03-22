module;
#include <mutex>
#include <QSysInfo>

#include <QString>
#include <QDateTime>

module Media.Info;


namespace ArtifactCore {

class MediaInfo::Impl {
private:
	QString name;
	int width_=0;
	int height_=0;

	long long duration_ = 0;
	QString title_;
	QString codecName_;

public:
	Impl();
	explicit Impl(const QString& name,int width,int height,long long duration);
	~Impl();
	int width() const;
	int height() const;
	long long duration() const;
	QString titleName() const;
	QString codecName() const;
	
	void clear();
};


int MediaInfo::Impl::width() const
{
	return width_;
}

int MediaInfo::Impl::height() const
{
	return height_;
}

long long MediaInfo::Impl::duration() const
{
	return duration_;
}

MediaInfo::Impl::Impl()
{

}

void MediaInfo::Impl::clear()
{

}

MediaInfo::Impl::~Impl()
{

}

QString MediaInfo::Impl::titleName() const
{
 return title_;
}

QString MediaInfo::Impl::codecName() const
{
 return codecName_;
}

MediaInfo::MediaInfo():impl_(new Impl())
{

}

MediaInfo::~MediaInfo()
{
 delete impl_;
}


int MediaInfo::width() const
{

	return impl_->width();
}

QString MediaInfo::title() const
{

 return QString();
}

QString MediaInfo::codecName() const
{
 return QString();
}

class MediaInfoBuilder::Impl {
private:

public:
 Impl();
 ~Impl();
 int32_t width_ = 0;
 int32_t height_ = 0;
 int32_t br_ = 0;
 QString title_;
 int64_t duration_ = 0;
 QDateTime creationTime_;
};

MediaInfoBuilder::Impl::Impl()
{

}

MediaInfoBuilder::Impl::~Impl()
{

}

MediaInfoBuilder& MediaInfoBuilder::setCreationTime(const QDateTime& dt)
{
 impl_->creationTime_ = dt;
 return *this;
}

MediaInfoBuilder& MediaInfoBuilder::setWidth(int w)
{
 impl_->width_;
 return *this;
}

MediaInfoBuilder& MediaInfoBuilder::setHeight(int h)
{
 impl_->height_;
 return *this;
}

MediaInfoBuilder& MediaInfoBuilder::setTitle(const QString& title)
{
 impl_->title_ = title;
 return *this;
}

MediaInfoBuilder& MediaInfoBuilder::setBitrate(int br)
{
 impl_->br_ = br;
 return *this;
}

MediaInfoBuilder& MediaInfoBuilder::setDuration(int64_t duration)
{
 impl_->duration_ = duration;
 return *this;
}

MediaInfo MediaInfoBuilder::build() const
{
 MediaInfo info;

 return info;
}

};