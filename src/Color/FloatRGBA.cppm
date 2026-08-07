module;
#include <utility>

module FloatRGBA;

namespace ArtifactCore {

FloatRGBA::operator FloatColor() const
{
 return FloatColor(r_, g_, b_, a_);
}
}
