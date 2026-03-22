module;

export module Composition.Context;

import Scene.SimulationSettings;

export namespace ArtifactCore
{
 /// コンポジション実行時の空間・シミュレーション系コンテキスト。
 struct CompositionContext
 {
  SimulationSettings simulation;
 };
}
