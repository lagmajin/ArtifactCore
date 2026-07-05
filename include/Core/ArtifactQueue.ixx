module;
#include <QQueue>
export module Core.ArtifactQueue;
export namespace ArtifactCore {

template <typename T>
class ArtifactQueue {
public:
    void enqueue(const T& item) { queue_.enqueue(item); }
    T dequeue() { return queue_.dequeue(); }
    T& head() { return queue_.head(); }
    const T& head() const { return queue_.head(); }
    bool isEmpty() const { return queue_.isEmpty(); }
    int size() const { return queue_.size(); }
    void clear() { queue_.clear(); }
private:
    QQueue<T> queue_;
};

} // namespace ArtifactCore
