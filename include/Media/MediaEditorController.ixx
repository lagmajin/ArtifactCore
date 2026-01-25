module;
#include <QString>
#include <QImage>
#include <QByteArray>
export module MediaEditorController;



import MediaPlaybackController;

namespace ArtifactCore {

class MediaEditorController {
private:
    MediaPlaybackController* playbackController_;

public:
    MediaEditorController();
    ~MediaEditorController();

    bool openMedia(const QString& filePath);
    void play();
    void pause();
    void stop();
    void seek(int64_t timeMs);

    QImage getCurrentFrame();
    QByteArray getCurrentAudio();
};

} // namespace ArtifactCore