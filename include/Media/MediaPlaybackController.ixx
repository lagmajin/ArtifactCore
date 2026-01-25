module;
#define QT_NO_KEYWORDS
#include <QString>
#include <QImage>
#include <QByteArray>
export module MediaPlaybackController;



import MediaSource;
import MediaReader;
import MediaImageFrameDecoder;
import MediaAudioDecoder;

export namespace ArtifactCore {

class MediaPlaybackController {
private:
    MediaSource* mediaSource_;
    MediaReader* mediaReader_;
    MediaImageFrameDecoder* videoDecoder_;
    MediaAudioDecoder* audioDecoder_;

    bool isPlaying_ = false;

public:
    MediaPlaybackController();
    ~MediaPlaybackController();

    bool openMedia(const QString& url);
    void closeMedia();

    void play();
    void pause();
    void stop();
    void seek(int64_t timestampMs);

    QImage getNextVideoFrame();
    QByteArray getNextAudioFrame();
};

} // namespace ArtifactCore