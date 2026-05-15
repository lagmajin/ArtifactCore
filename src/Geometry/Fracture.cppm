module;
#include <utility>

#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QVector2D>
#include <QVector3D>
#include <QLineF>
#include <QRandomGenerator>
#include <QString>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <limits>

import std;

module Geometry.Fracture;

namespace ArtifactCore {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

static float randomRange(std::mt19937& rng, float minValue, float maxValue) {
 if (maxValue <= minValue) {
  return minValue;
 }
 std::uniform_real_distribution<float> dist(minValue, maxValue);
 return dist(rng);
}

static QPointF clampPointToRect(const QPointF& point, const QRectF& rect) {
 return QPointF(
  std::clamp(point.x(), rect.left(), rect.right()),
  std::clamp(point.y(), rect.top(), rect.bottom()));
}

static QPointF rayRectIntersection(const QPointF& origin, float angle, const QRectF& rect) {
 const double dx = std::cos(angle);
 const double dy = std::sin(angle);
 double bestT = std::numeric_limits<double>::max();
 QPointF bestPoint = origin;

 auto consider = [&](double t) {
  if (t <= 0.0) {
   return;
  }
  const double x = origin.x() + dx * t;
  const double y = origin.y() + dy * t;
  if (x >= rect.left() - 0.0001 && x <= rect.right() + 0.0001 &&
      y >= rect.top() - 0.0001 && y <= rect.bottom() + 0.0001 &&
      t < bestT) {
   bestT = t;
   bestPoint = QPointF(x, y);
  }
 };

 if (std::abs(dx) > 1e-6) {
  consider((rect.left() - origin.x()) / dx);
  consider((rect.right() - origin.x()) / dx);
 }
 if (std::abs(dy) > 1e-6) {
  consider((rect.top() - origin.y()) / dy);
  consider((rect.bottom() - origin.y()) / dy);
 }

 return bestPoint;
}

static QRectF polygonBounds(const QPolygonF& polygon) {
 return polygon.boundingRect();
}

static QPointF polygonCentroid(const QPolygonF& polygon) {
 if (polygon.isEmpty()) {
  return QPointF();
 }

 double twiceArea = 0.0;
 double cx = 0.0;
 double cy = 0.0;

 for (int i = 0; i < polygon.size(); ++i) {
  const QPointF& p0 = polygon[i];
  const QPointF& p1 = polygon[(i + 1) % polygon.size()];
  const double cross = p0.x() * p1.y() - p1.x() * p0.y();
  twiceArea += cross;
  cx += (p0.x() + p1.x()) * cross;
  cy += (p0.y() + p1.y()) * cross;
 }

 if (std::abs(twiceArea) < 1e-6) {
  return polygon.boundingRect().center();
 }

 const double inv = 1.0 / (3.0 * twiceArea);
 return QPointF(cx * inv, cy * inv);
}

static float polygonArea(const QPolygonF& polygon) {
 if (polygon.size() < 3) {
  return 0.0f;
 }

 double area = 0.0;
 for (int i = 0; i < polygon.size(); ++i) {
  const QPointF& p0 = polygon[i];
  const QPointF& p1 = polygon[(i + 1) % polygon.size()];
  area += p0.x() * p1.y() - p1.x() * p0.y();
 }
 return static_cast<float>(std::abs(area) * 0.5);
}

static QPolygonF makeJitteredCell(const QRectF& cell, std::mt19937& rng, float jitterFactor) {
 const QPointF center = cell.center();
 const float jx = static_cast<float>(cell.width()) * jitterFactor;
 const float jy = static_cast<float>(cell.height()) * jitterFactor;

 QPolygonF poly;
 poly << QPointF(cell.left() + randomRange(rng, 0.0f, jx), cell.top() + randomRange(rng, 0.0f, jy));
 poly << QPointF(cell.right() - randomRange(rng, 0.0f, jx), cell.top() + randomRange(rng, 0.0f, jy));
 poly << QPointF(cell.right() - randomRange(rng, 0.0f, jx), cell.bottom() - randomRange(rng, 0.0f, jy));
 poly << QPointF(cell.left() + randomRange(rng, 0.0f, jx), cell.bottom() - randomRange(rng, 0.0f, jy));

 if (poly.boundingRect().isEmpty()) {
  poly << center;
 }

 return poly;
}

static std::vector<QPolygonF> generateGridPolygons(const QRectF& bounds,
                                                   int shardCount,
                                                   std::mt19937& rng,
                                                   float jitterFactor) {
 const int cols = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(shardCount)))));
 const int rows = std::max(1, static_cast<int>(std::ceil(static_cast<double>(shardCount) / cols)));
 const double cellW = bounds.width() / cols;
 const double cellH = bounds.height() / rows;

 std::vector<QPolygonF> polygons;
 polygons.reserve(static_cast<size_t>(rows * cols));

 int produced = 0;
 for (int y = 0; y < rows && produced < shardCount; ++y) {
  for (int x = 0; x < cols && produced < shardCount; ++x) {
   QRectF cell(bounds.left() + x * cellW,
               bounds.top() + y * cellH,
               cellW,
               cellH);
   polygons.push_back(makeJitteredCell(cell, rng, jitterFactor));
   ++produced;
  }
 }

 return polygons;
}

static std::vector<QPolygonF> generateRadialPolygons(const QRectF& bounds,
                                                     const QPointF& pivot,
                                                     int shardCount,
                                                     std::mt19937& rng,
                                                     float jitterFactor) {
 std::vector<float> angles;
 angles.reserve(static_cast<size_t>(shardCount));
 for (int i = 0; i < shardCount; ++i) {
  const float base = (kTwoPi * static_cast<float>(i)) / static_cast<float>(shardCount);
  const float jitter = randomRange(rng, -jitterFactor, jitterFactor);
  angles.push_back(base + jitter);
 }

 std::sort(angles.begin(), angles.end());

 std::vector<QPolygonF> polygons;
 polygons.reserve(static_cast<size_t>(shardCount));

 for (int i = 0; i < shardCount; ++i) {
  const float a0 = angles[i];
  const float a1 = angles[(i + 1) % shardCount] + ((i + 1 == shardCount) ? kTwoPi : 0.0f);
  const float mid = (a0 + a1) * 0.5f;

  const QPointF p0 = clampPointToRect(pivot, bounds);
  const QPointF p1 = rayRectIntersection(pivot, a0, bounds);
  const QPointF p2 = rayRectIntersection(pivot, a1, bounds);
  const QPointF inner = QPointF(
   pivot.x() + std::cos(mid) * std::min(bounds.width(), bounds.height()) * 0.12,
   pivot.y() + std::sin(mid) * std::min(bounds.width(), bounds.height()) * 0.12);

  QPolygonF poly;
  poly << p0 << inner << p1 << p2;
  polygons.push_back(poly);
 }

 return polygons;
}

static std::vector<QPolygonF> generateHybridPolygons(const QRectF& bounds,
                                                     const QPointF& pivot,
                                                     int shardCount,
                                                     std::mt19937& rng,
                                                     float jitterFactor) {
 std::vector<QPolygonF> polygons;
 const int radialCount = std::max(3, shardCount / 2);
 auto radial = generateRadialPolygons(bounds, pivot, radialCount, rng, jitterFactor);
 polygons.insert(polygons.end(), radial.begin(), radial.end());

 const int gridCount = std::max(1, shardCount - static_cast<int>(polygons.size()));
 auto grid = generateGridPolygons(bounds, gridCount, rng, jitterFactor * 0.8f);
 polygons.insert(polygons.end(), grid.begin(), grid.end());
 return polygons;
}

static QVector3D normalizeOrFallback(const QVector3D& v, const QVector3D& fallback) {
 const float len = v.length();
 if (len <= 1e-6f) {
  return fallback;
 }
 return v / len;
}

static QRectF meshBounds(const Mesh& mesh) {
 auto attr = mesh.vertexAttributes().get<QVector3D>("position");
 if (!attr || attr->data().isEmpty()) {
  attr = mesh.faceVertexAttributes().get<QVector3D>("position");
 }
 if (!attr || attr->data().isEmpty()) {
  return QRectF();
 }

 const auto& data = attr->data();
 float minX = data[0].x();
 float minY = data[0].y();
 float maxX = data[0].x();
 float maxY = data[0].y();
 for (const auto& p : data) {
  minX = std::min(minX, p.x());
  minY = std::min(minY, p.y());
  maxX = std::max(maxX, p.x());
  maxY = std::max(maxY, p.y());
 }
 return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}

static std::vector<QPolygonF> extractMeshFacePolygons(const Mesh& mesh) {
 std::vector<QPolygonF> polygons;

 auto positionAttr = mesh.vertexAttributes().get<QVector3D>("position");
 if (!positionAttr || positionAttr->data().isEmpty()) {
  return polygons;
 }

 const auto& positions = positionAttr->data();
 const int faces = mesh.polygonCount();
 polygons.reserve(static_cast<size_t>(faces));

 for (int faceIndex = 0; faceIndex < faces; ++faceIndex) {
  const QVector<int> vertices = mesh.getPolygonVertices(faceIndex);
  if (vertices.size() < 3) {
   continue;
  }

  QPolygonF polygon;
  polygon.reserve(vertices.size());
  for (int v : vertices) {
   if (v < 0 || v >= positions.size()) {
    continue;
   }
   const QVector3D& pos = positions[v];
   polygon << QPointF(pos.x(), pos.y());
  }

  if (polygon.size() >= 3) {
   polygons.push_back(polygon);
  }
 }

 return polygons;
}

static Particle makeDebrisParticle(const QPointF& origin,
                                   const FractureSettings& settings,
                                   std::mt19937& rng,
                                   float scaleHint) {
 Particle particle;
 particle.position = {static_cast<float>(origin.x()), static_cast<float>(origin.y()), 0.0f};
 particle.prevPosition = particle.position;

 const float angle = randomRange(rng, 0.0f, kTwoPi);
 const float speed = randomRange(rng, settings.impulseStrength * 0.18f, settings.impulseStrength * 0.55f) * scaleHint;
 particle.velocity = {
  std::cos(angle) * speed,
  std::sin(angle) * speed,
  randomRange(rng, -speed * 0.2f, speed * 0.2f)
 };
 particle.angularVelocity = {
  randomRange(rng, -settings.angularStrength, settings.angularStrength),
  randomRange(rng, -settings.angularStrength, settings.angularStrength),
  randomRange(rng, -settings.angularStrength, settings.angularStrength)
 };
 particle.size = randomRange(rng, 0.35f, 1.0f);
 particle.mass = randomRange(rng, 0.1f, 1.0f);
 particle.drag = 0.02f;
 particle.opacity = 1.0f;
 particle.lifetime = randomRange(rng, settings.debrisLifetimeMin, settings.debrisLifetimeMax);
 particle.color = {1.0f, 1.0f, 1.0f, 1.0f};
 return particle;
}

} // namespace

class FractureEffect::Impl {
public:
 std::shared_ptr<Mesh> sourceMesh;
 QRectF sourceBounds;
 FractureSettings settings;
 QPointF impactPoint;
 QVector3D impactNormal{0.0f, 0.0f, 1.0f};
 FractureResult result;
 QString error;
 bool hasExplicitBounds = false;
 bool hasImpactPoint = false;
 bool generated = false;
};

FractureEffect::FractureEffect() : impl_(std::make_unique<Impl>()) {}
FractureEffect::~FractureEffect() = default;

void FractureEffect::clear() {
 impl_->result = FractureResult{};
 impl_->error.clear();
 impl_->hasImpactPoint = false;
 impl_->generated = false;
}

void FractureEffect::setSourceMesh(std::shared_ptr<Mesh> mesh) {
 impl_->sourceMesh = std::move(mesh);
 impl_->generated = false;
}

std::shared_ptr<Mesh> FractureEffect::sourceMesh() const {
 return impl_->sourceMesh;
}

void FractureEffect::setSourceBounds(const QRectF& bounds) {
 impl_->sourceBounds = bounds;
 impl_->hasExplicitBounds = bounds.isValid() && !bounds.isEmpty();
 impl_->generated = false;
}

QRectF FractureEffect::sourceBounds() const {
 return impl_->sourceBounds;
}

void FractureEffect::setSettings(const FractureSettings& settings) {
 impl_->settings = settings;
 impl_->generated = false;
}

const FractureSettings& FractureEffect::settings() const {
 return impl_->settings;
}

void FractureEffect::setImpactPoint(const QPointF& point) {
 impl_->impactPoint = point;
 impl_->hasImpactPoint = true;
 impl_->generated = false;
}

QPointF FractureEffect::impactPoint() const {
 return impl_->impactPoint;
}

void FractureEffect::setImpactNormal(const QVector3D& normal) {
 impl_->impactNormal = normalizeOrFallback(normal, QVector3D(0.0f, 0.0f, 1.0f));
}

QVector3D FractureEffect::impactNormal() const {
 return impl_->impactNormal;
}

bool FractureEffect::generate() {
 impl_->error.clear();
 impl_->result = FractureResult{};
 impl_->generated = false;

 QRectF bounds = impl_->hasExplicitBounds ? impl_->sourceBounds : QRectF();
 if (!bounds.isValid() || bounds.isEmpty()) {
  if (impl_->sourceMesh) {
   bounds = meshBounds(*impl_->sourceMesh);
  }
 }

 if (!bounds.isValid() || bounds.isEmpty()) {
  impl_->error = QStringLiteral("fracture source bounds are empty");
  return false;
 }

 const int shardCount = std::max(1, impl_->settings.shardCount);
 std::mt19937 rng(impl_->settings.seed);
 const QPointF pivot = clampPointToRect(
  impl_->hasImpactPoint ? impl_->impactPoint : bounds.center(),
  bounds);

 std::vector<QPolygonF> polygons;
 if (impl_->sourceMesh) {
  auto meshPolygons = extractMeshFacePolygons(*impl_->sourceMesh);
  polygons.reserve(static_cast<size_t>(shardCount));
  for (const auto& polygon : meshPolygons) {
   if (static_cast<int>(polygons.size()) >= shardCount) {
    break;
   }
   if (polygon.size() >= 3) {
    polygons.push_back(polygon);
   }
  }
 }

 if (static_cast<int>(polygons.size()) < shardCount) {
  const int remaining = shardCount - static_cast<int>(polygons.size());
  std::vector<QPolygonF> generated;
  switch (impl_->settings.pattern) {
  case FracturePattern::Grid:
   generated = generateGridPolygons(bounds, remaining, rng, impl_->settings.cellJitter);
   break;
  case FracturePattern::Voronoi:
   generated = generateHybridPolygons(bounds, pivot, remaining, rng, impl_->settings.edgeJitter);
   break;
  case FracturePattern::Hybrid:
   generated = generateHybridPolygons(bounds, pivot, remaining, rng, impl_->settings.edgeJitter * 1.2f);
   break;
  case FracturePattern::Radial:
  default:
   generated = generateRadialPolygons(bounds, pivot, remaining, rng, impl_->settings.edgeJitter);
   break;
  }
  polygons.insert(polygons.end(), generated.begin(), generated.end());
 }

 if (polygons.empty()) {
  impl_->error = QStringLiteral("fracture generation produced no shards");
  return false;
 }

 impl_->result.sourceBounds = bounds;
 impl_->result.impactPoint = QVector3D(static_cast<float>(pivot.x()), static_cast<float>(pivot.y()), 0.0f);
 impl_->result.impactNormal = impl_->impactNormal;
 impl_->result.valid = true;
 impl_->result.shards.reserve(polygons.size());

  const float boundsWidth = static_cast<float>(bounds.width());
  const float boundsHeight = static_cast<float>(bounds.height());
  const float boundsDiag = std::max<float>(1.0f, std::sqrt(boundsWidth * boundsWidth + boundsHeight * boundsHeight));
 const int debrisCount = std::max(0, static_cast<int>(std::round(static_cast<float>(shardCount) * impl_->settings.debrisRatio)));

 for (size_t i = 0; i < polygons.size(); ++i) {
  const QPolygonF& poly = polygons[i];
  if (poly.size() < 3) {
   continue;
  }

  FractureShard shard;
  shard.sourceIndex = static_cast<int>(i);
  shard.polygon = poly;
  shard.localBounds = polygonBounds(poly);
  const QPointF centroid2D = polygonCentroid(poly);
  const float area = polygonArea(poly);
  shard.sourceCentroid = QVector3D(static_cast<float>(centroid2D.x()), static_cast<float>(centroid2D.y()), 0.0f);
  shard.position = shard.sourceCentroid;
   shard.mass = std::max<float>(0.05f, area / std::max<float>(1.0f, boundsWidth * boundsHeight));
  shard.lifetime = randomRange(rng, impl_->settings.lifetimeMin, impl_->settings.lifetimeMax);
  shard.opacity = 1.0f;
  shard.scale = 1.0f;

  QVector3D away = normalizeOrFallback(
   shard.sourceCentroid - impl_->result.impactPoint,
   QVector3D(0.0f, -1.0f, 0.0f));
  const QPointF localCenter = shard.localBounds.center();
  const float distToPivot = QLineF(localCenter, pivot).length();
  const float distNorm = std::clamp(
   distToPivot / boundsDiag,
   0.0f,
   1.0f);
  const float speed = impl_->settings.impulseStrength * (0.45f + distNorm * 0.85f);
  shard.velocity = away * speed;
  shard.velocity.setZ(randomRange(rng, -speed * 0.03f, speed * 0.03f));
  shard.angularVelocity = QVector3D(
   randomRange(rng, -impl_->settings.angularStrength, impl_->settings.angularStrength),
   randomRange(rng, -impl_->settings.angularStrength, impl_->settings.angularStrength),
   randomRange(rng, -impl_->settings.angularStrength, impl_->settings.angularStrength));
  if (impl_->settings.protectedCenterRadius > 0.0f && distToPivot < impl_->settings.protectedCenterRadius) {
   shard.velocity *= 0.25f;
   shard.angularVelocity *= 0.25f;
   shard.scale = 0.15f;
  }
  shard.shardId = Id();
  impl_->result.shards.push_back(std::move(shard));
 }

  const float debrisScale = std::max<float>(0.25f, std::min<float>(boundsWidth, boundsHeight) / 1024.0f);
 impl_->result.debris.reserve(static_cast<size_t>(debrisCount));
 for (int i = 0; i < debrisCount; ++i) {
  impl_->result.debris.push_back(makeDebrisParticle(pivot, impl_->settings, rng, debrisScale));
 }

 impl_->generated = true;
 return true;
}

void FractureEffect::update(float deltaSeconds) {
 if (deltaSeconds <= 0.0f || !impl_->generated) {
  return;
 }

 for (auto& shard : impl_->result.shards) {
  if (!shard.active) {
   continue;
  }

  shard.age += deltaSeconds;
  if (shard.age >= shard.lifetime) {
   shard.active = false;
   shard.opacity = 0.0f;
   continue;
  }

  shard.position += shard.velocity * deltaSeconds;
  shard.velocity.setY(shard.velocity.y() + impl_->settings.gravity * deltaSeconds);
  shard.velocity *= impl_->settings.damping;
  shard.rotation += shard.angularVelocity.z() * deltaSeconds;
  shard.opacity = std::max(0.0f, 1.0f - (shard.age / std::max(0.001f, shard.lifetime)));
 }

 for (auto& particle : impl_->result.debris) {
  particle.age += deltaSeconds;
  if (particle.age >= particle.lifetime) {
   particle.opacity = 0.0f;
   continue;
  }
  particle.prevPosition = particle.position;
  particle.position.x += particle.velocity.x * deltaSeconds;
  particle.position.y += particle.velocity.y * deltaSeconds;
  particle.position.z += particle.velocity.z * deltaSeconds;
  particle.velocity.y += impl_->settings.gravity * deltaSeconds;
  particle.velocity.x *= impl_->settings.damping;
  particle.velocity.y *= impl_->settings.damping;
  particle.velocity.z *= impl_->settings.damping;
  particle.opacity = std::max(0.0f, 1.0f - (particle.age / std::max(0.001f, particle.lifetime)));
 }
}

bool FractureEffect::hasResult() const {
 return impl_->generated && impl_->result.valid;
}

const FractureResult& FractureEffect::result() const {
 return impl_->result;
}

const std::vector<FractureShard>& FractureEffect::shards() const {
 return impl_->result.shards;
}

const std::vector<Particle>& FractureEffect::debris() const {
 return impl_->result.debris;
}

QString FractureEffect::errorString() const {
 return impl_->error;
}

QRectF computeFractureSourceBounds(const Mesh& mesh) {
 return meshBounds(mesh);
}

QPolygonF makeFracturePolygonFromRadialCell(const QPointF& pivot,
                                           const QRectF& bounds,
                                           float startAngle,
                                           float endAngle,
                                           float innerInset,
                                           float outerInset) {
 const QPointF clampedPivot = clampPointToRect(pivot, bounds);
 const float innerRadius = std::max(0.0f, innerInset);
 const QPointF inner0(clampedPivot.x() + std::cos(startAngle) * innerRadius,
                      clampedPivot.y() + std::sin(startAngle) * innerRadius);
 const QPointF inner1(clampedPivot.x() + std::cos(endAngle) * innerRadius,
                      clampedPivot.y() + std::sin(endAngle) * innerRadius);
 const QPointF outer0 = rayRectIntersection(clampedPivot, startAngle, bounds);
 const QPointF outer1 = rayRectIntersection(clampedPivot, endAngle, bounds);
 const float blend = std::max(0.0f, outerInset);
 const QPointF midOuter(
  clampedPivot.x() + std::cos((startAngle + endAngle) * 0.5f) * blend,
  clampedPivot.y() + std::sin((startAngle + endAngle) * 0.5f) * blend);

 QPolygonF poly;
 poly << clampedPivot << inner0 << outer0 << midOuter << outer1 << inner1;
 return poly;
}

FractureSettings makeFracturePreset(FracturePreset preset) {
 FractureSettings settings;
 switch (preset) {
 case FracturePreset::Glass:
  settings.pattern = FracturePattern::Radial;
  settings.shardCount = 28;
  settings.debrisCount = 90;
  settings.impulseStrength = 180.0f;
  settings.angularStrength = 12.0f;
  settings.lifetimeMin = 0.45f;
  settings.lifetimeMax = 1.6f;
  settings.debrisLifetimeMin = 0.15f;
  settings.debrisLifetimeMax = 0.9f;
  settings.edgeJitter = 0.22f;
  settings.debrisRatio = 0.65f;
  settings.protectedCenterRadius = 24.0f;
  break;
 case FracturePreset::Concrete:
  settings.pattern = FracturePattern::Hybrid;
  settings.shardCount = 20;
  settings.debrisCount = 70;
  settings.impulseStrength = 135.0f;
  settings.angularStrength = 8.0f;
  settings.lifetimeMin = 0.8f;
  settings.lifetimeMax = 2.6f;
  settings.debrisLifetimeMin = 0.2f;
  settings.debrisLifetimeMax = 1.4f;
  settings.edgeJitter = 0.14f;
  settings.cellJitter = 0.18f;
  settings.debrisRatio = 0.45f;
  settings.protectedCenterRadius = 18.0f;
  break;
 case FracturePreset::Stone:
  settings.pattern = FracturePattern::Grid;
  settings.shardCount = 14;
  settings.debrisCount = 32;
  settings.impulseStrength = 95.0f;
  settings.angularStrength = 5.0f;
  settings.lifetimeMin = 1.0f;
  settings.lifetimeMax = 3.2f;
  settings.debrisLifetimeMin = 0.25f;
  settings.debrisLifetimeMax = 1.2f;
  settings.edgeJitter = 0.08f;
  settings.cellJitter = 0.10f;
  settings.debrisRatio = 0.28f;
  settings.protectedCenterRadius = 10.0f;
  break;
 case FracturePreset::Metal:
  settings.pattern = FracturePattern::Radial;
  settings.shardCount = 12;
  settings.debrisCount = 18;
  settings.impulseStrength = 75.0f;
  settings.angularStrength = 4.0f;
  settings.lifetimeMin = 1.6f;
  settings.lifetimeMax = 4.0f;
  settings.debrisLifetimeMin = 0.35f;
  settings.debrisLifetimeMax = 1.6f;
  settings.edgeJitter = 0.04f;
  settings.cellJitter = 0.05f;
  settings.debrisRatio = 0.18f;
  settings.protectedCenterRadius = 8.0f;
  settings.damping = 0.992f;
  break;
 case FracturePreset::Wood:
  settings.pattern = FracturePattern::Grid;
  settings.shardCount = 10;
  settings.debrisCount = 24;
  settings.impulseStrength = 110.0f;
  settings.angularStrength = 6.5f;
  settings.lifetimeMin = 0.9f;
  settings.lifetimeMax = 2.2f;
  settings.debrisLifetimeMin = 0.2f;
  settings.debrisLifetimeMax = 1.1f;
  settings.edgeJitter = 0.11f;
  settings.cellJitter = 0.15f;
  settings.debrisRatio = 0.25f;
  settings.protectedCenterRadius = 14.0f;
  break;
 case FracturePreset::Dust:
  settings.pattern = FracturePattern::Voronoi;
  settings.shardCount = 40;
  settings.debrisCount = 180;
  settings.impulseStrength = 55.0f;
  settings.angularStrength = 16.0f;
  settings.lifetimeMin = 0.18f;
  settings.lifetimeMax = 0.75f;
  settings.debrisLifetimeMin = 0.08f;
  settings.debrisLifetimeMax = 0.45f;
  settings.edgeJitter = 0.28f;
  settings.cellJitter = 0.22f;
  settings.debrisRatio = 0.9f;
  settings.protectedCenterRadius = 6.0f;
  settings.damping = 0.975f;
  break;
 }
 return settings;
}

} // namespace ArtifactCore
