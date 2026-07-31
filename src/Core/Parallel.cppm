module;
#include <tbb/parallel_for.h>

module Core.Parallel;

namespace ArtifactCore {

void Parallel::ForErased(int start, int end, const std::function<void(int)>& func) {
    if (start >= end || !func) return;
    tbb::parallel_for(start, end, func);
}

}
