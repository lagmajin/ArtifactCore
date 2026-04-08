module;
#include <utility>

#include "../Define/DllExportMacro.hpp"

#include <memory>
#include <vector>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QVector2D>
#include <QVector3D>
export module Geometry.Fracture;

import Mesh;
import Particle;
import Utils.Id;

export namespace ArtifactCore {

enum class FracturePattern : quint8 {
 Radial = 0,
 Grid,
 Voronoi,
 Hybrid,
};

enum class FracturePreset : quint8 {
 Glass = 0,
 Concrete,
 Stone,
 Metal,
 Wood,
 Dust,
};

struct LIBRARY_DLL_API FractureSettings {
 FracturePattern pattern = FracturePattern::Radial;
 int shardCount = 16;
 int debrisCount = 48;
 float impulseStrength = 120.0f;
 float angularStrength = 8.0f;
 float gravity = 0.0f;
 float damping = 0.985f;
 float lifetimeMin = 0.8f;
 float lifetimeMax = 2.5f;
 float debrisLifetimeMin = 0.25f;
 float debrisLifetimeMax = 1.2f;
 float impactRadius = 96.0f;
 float edgeJitter = 0.12f;
 float cellJitter = 0.18f;
 float debrisRatio = 0.35f;
 float protectedCenterRadius = 0.0f;
 quint32 seed = 0;
 bool preserveSourceFill = true;
};

struct LIBRARY_DLL_API FractureShard {
 Id shardId;
 int sourceIndex = -1;
 QPolygonF polygon;
 QRectF localBounds;
 QVector3D sourceCentroid{0.0f, 0.0f, 0.0f};
 QVector3D position{0.0f, 0.0f, 0.0f};
 QVector3D velocity{0.0f, 0.0f, 0.0f};
 QVector3D angularVelocity{0.0f, 0.0f, 0.0f};
 float rotation = 0.0f;
 float scale = 1.0f;
 float opacity = 1.0f;
 float age = 0.0f;
 float lifetime = 1.0f;
 float mass = 1.0f;
 bool active = true;
 bool debris = false;
};

struct LIBRARY_DLL_API FractureResult {
 QRectF sourceBounds;
 QVector3D impactPoint{0.0f, 0.0f, 0.0f};
 QVector3D impactNormal{0.0f, 0.0f, 1.0f};
 std::vector<FractureShard> shards;
 std::vector<Particle> debris;
 bool valid = false;
};

class LIBRARY_DLL_API FractureEffect {
public:
 FractureEffect();
 ~FractureEffect();

 FractureEffect(const FractureEffect&) = delete;
 FractureEffect& operator=(const FractureEffect&) = delete;

 void clear();

 void setSourceMesh(std::shared_ptr<Mesh> mesh);
 std::shared_ptr<Mesh> sourceMesh() const;

 void setSourceBounds(const QRectF& bounds);
 QRectF sourceBounds() const;

 void setSettings(const FractureSettings& settings);
 const FractureSettings& settings() const;

 void setImpactPoint(const QPointF& point);
 QPointF impactPoint() const;

 void setImpactNormal(const QVector3D& normal);
 QVector3D impactNormal() const;

 bool generate();
 void update(float deltaSeconds);

 bool hasResult() const;
 const FractureResult& result() const;
 const std::vector<FractureShard>& shards() const;
 const std::vector<Particle>& debris() const;

 QString errorString() const;

private:
 class Impl;
 std::unique_ptr<Impl> impl_;
};

LIBRARY_DLL_API QRectF computeFractureSourceBounds(const Mesh& mesh);
LIBRARY_DLL_API QPolygonF makeFracturePolygonFromRadialCell(
 const QPointF& pivot,
 const QRectF& bounds,
 float startAngle,
 float endAngle,
 float innerInset,
 float outerInset);
LIBRARY_DLL_API FractureSettings makeFracturePreset(FracturePreset preset);

} // namespace ArtifactCore
