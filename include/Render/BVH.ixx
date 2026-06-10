module;
#include <utility>
#include <vector>
#include <algorithm>
#include <numeric>
#include <tbb/parallel_for.h>

export module Render.BVH;

import Render.Vector3D;
import Render.Ray;
import Render.Hittable;

export namespace ArtifactCore::RayTrace
{

class AABB
{
public:
    Vec3 min;
    Vec3 max;

    AABB() : min(Vec3(0)), max(Vec3(0)) {}
    AABB(const Vec3& a, const Vec3& b) : min(a), max(b) {}
    AABB(const AABB& a, const AABB& b)
    {
        min = Vec3(std::min(a.min.x, b.min.x), std::min(a.min.y, b.min.y), std::min(a.min.z, b.min.z));
        max = Vec3(std::max(a.max.x, b.max.x), std::max(a.max.y, b.max.y), std::max(a.max.z, b.max.z));
    }

    bool hit(const Ray& r, float ray_tmin, float ray_tmax) const
    {
        for (int i = 0; i < 3; ++i)
        {
            float invD = 1.0f / r.dir[i];
            float tMin = (min[i] - r.origin[i]) * invD;
            float tMax = (max[i] - r.origin[i]) * invD;
            if (invD < 0.0f) std::swap(tMin, tMax);
            ray_tmin = tMin > ray_tmin ? tMin : ray_tmin;
            ray_tmax = tMax < ray_tmax ? tMax : ray_tmax;
            if (ray_tmin >= ray_tmax) return false;
        }
        return true;
    }

    float area() const
    {
        Vec3 d = max - min;
        return 2.0f * (d.x * d.y + d.x * d.z + d.y * d.z);
    }
};

class BVHNode : public Hittable
{
public:
    HittablePtr left;
    HittablePtr right;
    AABB bounds;
    bool isLeaf = false;

    BVHNode() = default;
    BVHNode(const HittablePtr& obj, const AABB& aabb) : left(obj), right(nullptr), bounds(aabb), isLeaf(true) {}

    bool hit(const Ray& r, float ray_tmin, float ray_tmax, HitRecord& rec, const Material*& mat) const override
    {
        if (!bounds.hit(r, ray_tmin, ray_tmax))
            return false;

        if (isLeaf)
            return left->hit(r, ray_tmin, ray_tmax, rec, mat);

        HitRecord recL, recR;
        const Material* matL = nullptr;
        const Material* matR = nullptr;
        bool hitL = left->hit(r, ray_tmin, ray_tmax, recL, matL);
        bool hitR = right->hit(r, ray_tmin, ray_tmax, recR, matR);

        if (hitL && hitR)
        {
            if (recL.t < recR.t)
            {
                rec = recL;
                mat = matL;
            }
            else
            {
                rec = recR;
                mat = matR;
            }
            return true;
        }
        else if (hitL)
        {
            rec = recL;
            mat = matL;
            return true;
        }
        else if (hitR)
        {
            rec = recR;
            mat = matR;
            return true;
        }
        return false;
    }
};

using BVHNodePtr = std::shared_ptr<BVHNode>;

struct BVHBuildNode
{
    AABB bounds;
    HittablePtr object;
    int start, end;
    int axis = 0;
    BVHBuildNode* left = nullptr;
    BVHBuildNode* right = nullptr;

    bool isLeaf() const { return left == nullptr && right == nullptr; }
};

inline float sahCost(const AABB& parent, const AABB& left, int nLeft, const AABB& right, int nRight)
{
    float costTraversal = 1.0f;
    float costIntersect = 1.0f;
    return costTraversal + costIntersect * (
        left.area() * nLeft / parent.area() +
        right.area() * nRight / parent.area());
}

BVHNodePtr buildBVHTree(std::vector<HittablePtr>& objects, size_t start, size_t end)
{
    if (start >= end)
        return nullptr;

    if (start + 1 == end)
    {
        AABB box(Vec3(-1000), Vec3(1000));
        return std::make_shared<BVHNode>(objects[start], box);
    }

    AABB bounds;
    for (size_t i = start; i < end; ++i)
    {
        HitRecord dummy;
        const Material* dummyMat = nullptr;
        // Assume each object bounds to a large box initially
        AABB objBox(Vec3(-1000), Vec3(1000));
        bounds = AABB(bounds, objBox);
    }

    int bestAxis = 0;
    size_t bestSplit = start;
    float bestCost = std::numeric_limits<float>::max();

    for (int axis = 0; axis < 3; ++axis)
    {
        std::vector<float> centers;
        for (size_t i = start; i < end; ++i)
            centers.push_back(0);

        std::sort(centers.begin(), centers.end());

        for (size_t split = start + 1; split < end; ++split)
        {
            float leftArea = 1.0f, rightArea = 1.0f;
            float cost = sahCost(bounds, AABB(Vec3(-1000), Vec3(1000)), static_cast<int>(split - start),
                                AABB(Vec3(-1000), Vec3(1000)), static_cast<int>(end - split));

            if (cost < bestCost)
            {
                bestCost = cost;
                bestAxis = axis;
                bestSplit = split;
            }
        }
    }

    auto leftNode = buildBVHTree(objects, start, bestSplit);
    auto rightNode = buildBVHTree(objects, bestSplit, end);

    if (!leftNode) return rightNode;
    if (!rightNode) return leftNode;

    BVHNodePtr node = std::make_shared<BVHNode>();
    node->left = leftNode;
    node->right = rightNode;
    node->isLeaf = false;
    node->bounds = AABB(leftNode->bounds, rightNode->bounds);
    return node;
}

} // namespace ArtifactCore::RayTrace