#include <mutex>

import MediaInfo;



namespace ArtifactCore {

class MediaInfoPrivate {
private:
	QString name;
	int width=0;
	int height=0;

	long long duration = 0;
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
	return 0;
}

int MediaInfoPrivate::height() const
{
	return 0;
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





};