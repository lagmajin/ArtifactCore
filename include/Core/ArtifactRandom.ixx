module;
#include <QRandomGenerator>
export module Core.ArtifactRandom;
export namespace ArtifactCore {

class ArtifactRandom {
public:
    static int between(int min, int max) {
        return QRandomGenerator::global()->bounded(min, max + 1);
    }
    static double between(double min, double max) {
        return QRandomGenerator::global()->generateDouble() * (max - min) + min;
    }
    static double unit() {
        return QRandomGenerator::global()->generateDouble();
    }
    static int next() {
        return static_cast<int>(QRandomGenerator::global()->generate());
    }
};

} // namespace ArtifactCore
