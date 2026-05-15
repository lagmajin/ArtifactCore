module;
#include <utility>
export module Graphics.Vector2D;

export namespace ArtifactCore
{
 struct Vex2
 {
  float x;
  float y;
  // Keep as plain POD for ABI-stable trivial copy/move.
 };

};
