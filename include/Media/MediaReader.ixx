module;
#define QT_NO_KEYWORDS
#include <QMutex>
#include <QWaitCondition>

extern "C" {
#include <libavformat/avformat.h>
}

#include <tbb/concurrent_queue.h>
#include <tbb/task_group.h>
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
    tbb::concurrent_queue<AVPacket*> videoQueue_;
    tbb::concurrent_queue<AVPacket*> audioQueue_;
    tbb::task_group taskGroup_;
    QMutex mutex_;
    QWaitCondition condition_;
    bool isRunning_ = false;
    bool isPaused_ = false;
    int videoStreamIndex_ = -1;
    int audioStreamIndex_ = -1;

public:
    explicit MediaReader(MediaSource* source);
    ~MediaReader();

    void start();
    void pause();
    void stop();

    AVPacket* getNextPacket(StreamType type);

private:
    void readLoop();
};

} // namespace ArtifactCore