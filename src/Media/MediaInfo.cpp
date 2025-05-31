#include <mutex>
#include <QSysInfo>

#include <QtCore/QString>
import MediaInfo;



namespace ArtifactCore {

class MediaInfoPrivate {
private:
	QString name;
	int width_=0;
	int height_=0;

	long long duration_ = 0;
public:
	MediaInfoPrivate();
	explicit MediaInfoPrivate(const QString& name,int width,int height,long long duration);
	~MediaInfoPrivate();
	int width() const;
	int height() const;
	long long duration() const;
	void clear();
};

MediaInfoPrivate::MediaInfoPrivate()
{

}


MediaInfoPrivate::MediaInfoPrivate(const QString& name, int width, int height, long long duration)
{

}

MediaInfoPrivate::~MediaInfoPrivate()
{

}

int MediaInfoPrivate::width() const
{
	return width_;
}

int MediaInfoPrivate::height() const
{
	return height_;
}

long long MediaInfoPrivate::duration() const
{
	return 0;
}

void MediaInfoPrivate::clear()
{

}

MediaInfo::MediaInfo()
{

}

MediaInfo::~MediaInfo()
{

}


int MediaInfo::width() const
{

	return 0;
}




};