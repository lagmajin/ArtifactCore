module;
#include <utility>
#include <vector>

export module Render.RenderableObject;

import Render.Vector3D;
import Render.Hittable;

export namespace ArtifactCore::RayTrace
{

class RenderableObject
{
public:
    Vec3 position;
    Vec3 rotation;
    Vec3 scale = Vec3(1, 1, 1);
    uint32_t id = 0;
    bool visible = true;

    RenderableObject() = default;
    RenderableObject(const Vec3& pos) : position(pos) {}

    virtual ~RenderableObject() = default;

    virtual std::vector<HittablePtr> toHittable() const = 0;
    virtual void updateTransform() {}
};

} // namespace ArtifactCore::RayTrace