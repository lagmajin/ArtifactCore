module;
#include <QSet>
export module Core.ArtifactSet;
export namespace ArtifactCore {

template <typename T>
class ArtifactSet {
public:
    void add(const T& item) { set_.insert(item); }
    void remove(const T& item) { set_.remove(item); }
    bool contains(const T& item) const { return set_.contains(item); }
    int size() const { return set_.size(); }
    bool isEmpty() const { return set_.isEmpty(); }
    void clear() { set_.clear(); }
    QSet<T> raw() const { return set_; }
    auto begin() { return set_.begin(); }
    auto end() { return set_.end(); }
    auto begin() const { return set_.begin(); }
    auto end() const { return set_.end(); }
private:
    QSet<T> set_;
};

} // namespace ArtifactCore
