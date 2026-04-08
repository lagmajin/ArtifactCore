module;
#include <utility>
#include <QPointF>
#include <QSizeF>
export module Scene.SimulationSettings;

export namespace ArtifactCore
{
 /// シーン全体で共有する物理空間の基準。
 struct SceneSpaceSettings
 {
  QPointF origin {0.0, 0.0};
  QSizeF extent {0.0, 0.0};
  double unitScale = 1.0;
 };

 /// 物理シミュレーション共通設定。
 struct PhysicsSpaceSettings
 {
  QPointF gravity {0.0, 9.8};
  int subSteps = 8;
  bool enabled = false;
 };

 /// 群集や多数オブジェクトの挙動調整用設定。
 struct CrowdSettings
 {
  float density = 1.0f;
  float cohesion = 0.5f;
  float separation = 0.5f;
  float alignment = 0.5f;
  float maxSpeed = 1.0f;
  float jitter = 0.1f;
  bool enabled = false;
 };

 /// シーン実行時のシミュレーション設定の束。
 struct SimulationSettings
 {
  SceneSpaceSettings space;
  PhysicsSpaceSettings physics;
  CrowdSettings crowd;
  double timeScale = 1.0;
 };
}
