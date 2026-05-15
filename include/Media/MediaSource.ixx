module;
#include <utility>
#define QT_NO_KEYWORDS
#include <QString>
#include <QObject>

extern "C" {
#include <libavformat/avformat.h>
}
export module MediaSource;

export namespace ArtifactCore {

class MediaSource {
private:
    AVFormatContext* formatContext_ = nullptr;
    AVIOContext* ioContext_ = nullptr;
    QString url_;
    QString lastError_;

public:
    MediaSource();
    ~MediaSource();

    bool open(const QString& url);
    bool seek(int64_t timestampMs);
    void close();
    bool isOpen() const { return formatContext_ != nullptr; }

    AVFormatContext* getFormatContext() const { return formatContext_; }
    const QString& getUrl() const { return url_; }
    const QString& getLastError() const { return lastError_; }
};

} // namespace ArtifactCore
