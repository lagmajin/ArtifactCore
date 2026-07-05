module;
#include <QElapsedTimer>
export module Core.ArtifactTimer;
export namespace ArtifactCore {

class ArtifactTimer {
public:
    void start() { timer_.start(); }
    qint64 elapsedMs() const { return timer_.elapsed(); }
    qint64 elapsedNs() const { return timer_.nsecsElapsed(); }
    bool hasExpired(qint64 timeoutMs) const { return timer_.elapsed() >= timeoutMs; }
    void restart() { timer_.restart(); }
    bool isValid() const { return timer_.isValid(); }
private:
    QElapsedTimer timer_;
};

} // namespace ArtifactCore
