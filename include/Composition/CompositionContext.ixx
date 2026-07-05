module;
#include <utility>

export module Composition.Context;

import Scene.SimulationSettings;

export namespace ArtifactCore
{
 /// コンポジション実行時の空間・シミュレーション系コンテキスト。
 struct CompositionContext
 {
  SimulationSettings simulation;

  SimulationSettings& simulationSettings() { return simulation; }
  const SimulationSettings& simulationSettings() const { return simulation; }

  SceneSpaceSettings& spaceSettings() { return simulation.space; }
  const SceneSpaceSettings& spaceSettings() const { return simulation.space; }

  PhysicsSpaceSettings& physicsSettings() { return simulation.physics; }
  const PhysicsSpaceSettings& physicsSettings() const { return simulation.physics; }
 };
}
