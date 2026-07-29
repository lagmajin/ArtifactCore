module;
#include <utility>
#include <deque>
#include <thread>
#define QT_NO_KEYWORDS
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>

extern "C" {
#include <libavformat/avformat.h>
}

export module MediaReader;

import MediaSource;

export namespace ArtifactCore {

enum class StreamType {
    Video,
    Audio,
    Unknown
};

class MediaReader {
private:
    MediaSource* mediaSource_;
    std::deque<AVPacket*> videoQueue_;
    std::deque<AVPacket*> audioQueue_;
    std::thread workerThread_;
    QMutex mutex_;
    QWaitCondition condition_;
    bool isRunning_ = false;
    bool isPaused_ = false;
    int videoStreamIndex_ = -1;
    int audioStreamIndex_ = -1;

public:
    explicit MediaReader(MediaSource* source);
    ~MediaReader();

    MediaReader(const MediaReader&) = delete;
    MediaReader& operator=(const MediaReader&) = delete;

    void start();
    void pause();
    void stop();

    AVPacket* getNextPacket(StreamType type);

private:
    void readLoop();
};

} // namespace ArtifactCore
