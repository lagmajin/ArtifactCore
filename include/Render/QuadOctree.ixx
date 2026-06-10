module;
#include <utility>
#include <vector>
#include <memory>

export module Render.QuadOctree;

import Render.Vector3D;
import Render.Ray;
import Render.Hittable;

export namespace ArtifactCore::RayTrace
{

struct QuadCell
{
    float minX, minY, minZ;
    float maxX, maxY, maxZ;
    std::vector<HittablePtr> objects;
    std::unique_ptr<QuadCell> children[4];

    QuadCell() : minX(0), minY(0), minZ(0), maxX(0), maxY(0), maxZ(0) {}
    QuadCell(float xmin, float ymin, float zmin, float xmax, float ymax, float zmax)
        : minX(xmin), minY(ymin), minZ(zmin), maxX(xmax), maxY(ymax), maxZ(zmax) {}

    bool isLeaf() const { return children[0] == nullptr; }

    void subdivide()
    {
        float midX = (minX + maxX) * 0.5f;
        float midY = (minY + maxY) * 0.5f;
        float midZ = (minZ + maxZ) * 0.5f;

        children[0] = std::make_unique<QuadCell>(minX, minY, minZ, midX, midY, midZ);
        children[1] = std::make_unique<QuadCell>(midX, minY, minZ, maxX, midY, midZ);
        children[2] = std::make_unique<QuadCell>(minX, midY, minZ, midX, maxY, midZ);
        children[3] = std::make_unique<QuadCell>(midX, midY, minZ, maxX, maxY, midZ);
    }

    bool contains(const Vec3& p) const
    {
        return p.x >= minX && p.x < maxX &&
               p.y >= minY && p.y < maxY &&
               p.z >= minZ && p.z < maxZ;
    }

    bool hitAABB(const Ray& r, float ray_tmin, float ray_tmax) const
    {
        for (int i = 0; i < 3; ++i)
        {
            float invD = 1.0f / r.dir[i];
            float tMin = 0, tMax = 0;
            if (i == 0) { tMin = (minX - r.origin.x) * invD; tMax = (maxX - r.origin.x) * invD; }
            if (i == 1) { tMin = (minY - r.origin.y) * invD; tMax = (maxY - r.origin.y) * invD; }
            if (i == 2) { tMin = (minZ - r.origin.z) * invD; tMax = (maxZ - r.origin.z) * invD; }

            if (invD < 0.0f) std::swap(tMin, tMax);
            ray_tmin = tMin > ray_tmin ? tMin : ray_tmin;
            ray_tmax = tMax < ray_tmax ? tMax : ray_tmax;
            if (ray_tmin >= ray_tmax) return false;
        }
        return true;
    }
};

class QuadOctree : public Hittable
{
public:
    std::unique_ptr<QuadCell> root;
    float worldSize = 1000.0f;

    QuadOctree()
    {
        root = std::make_unique<QuadCell>(-worldSize, -worldSize, -worldSize, worldSize, worldSize, worldSize);
    }

    QuadOctree(float size) : worldSize(size)
    {
        root = std::make_unique<QuadCell>(-size, -size, -size, size, size, size);
    }

    void add(const HittablePtr& obj, int depth = 0)
    {
        insert(root.get(), obj, depth);
    }

    void insert(QuadCell* node, const HittablePtr& obj, int depth)
    {
        if (!node->isLeaf() && depth > 0)
        {
            Vec3 center(0, 0, 0);
            for (int i = 0; i < 4; ++i)
            {
                if (node->children[i] && node->children[i]->contains(center))
                {
                    insert(node->children[i].get(), obj, depth - 1);
                    return;
                }
            }
        }

        if (node->isLeaf())
        {
            node->objects.push_back(obj);
            if (node->objects.size() > 10 && depth > 0)
            {
                node->subdivide();
                for (auto& existingObj : node->objects)
                {
                    for (int i = 0; i < 4; ++i)
                        insert(node->children[i].get(), existingObj, depth - 1);
                }
                node->objects.clear();
            }
        }
    }

    bool hit(const Ray& r, float ray_tmin, float ray_tmax, HitRecord& rec, const Material*& mat) const override
    {
        HitRecord tmp;
        const Material* tmpMat = nullptr;
        float closestSoFar = ray_tmax;
        bool hitAnything = false;

        hitCell(root.get(), r, ray_tmin, closestSoFar, tmp, tmpMat);

        if (hitAnything)
        {
            rec = tmp;
            mat = tmpMat;
        }
        return hitAnything;
    }

    void hitCell(const QuadCell* node, const Ray& r, float ray_tmin, float& closestSoFar, HitRecord& rec, const Material*& mat) const
    {
        if (!node->hitAABB(r, ray_tmin, closestSoFar))
            return;

        for (const auto& obj : node->objects)
        {
            HitRecord tmp;
            const Material* tmpMat = nullptr;
            if (obj->hit(r, ray_tmin, closestSoFar, tmp, tmpMat))
            {
                closestSoFar = tmp.t;
                rec = tmp;
                mat = tmpMat;
            }
        }

        for (int i = 0; i < 4; ++i)
        {
            if (node->children[i])
                hitCell(node->children[i].get(), r, ray_tmin, closestSoFar, rec, mat);
        }
    }
};

} // namespace ArtifactCore::RayTrace