module;
export module Graphics.Vector2D;

export namespace ArtifactCore
{
 struct Vex2
 {
  float x;
  float y;
  // 何も初期化しない。コンストラクタも定義しない。
  // = 完全 POD, trivially copyable, 最速。
 };

};