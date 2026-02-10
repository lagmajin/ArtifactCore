module;
export module Particle;

import std;

export namespace ArtifactCore
{
 struct float2 {
  float x, y;
 };

 struct float3 {
  float x, y, z;
 };

 struct float4 {
  float x, y, z, w;
 };
	
 struct Particle {
  std::uint64_t id = 0;
  std::uint32_t seed = 0;
  std::uint32_t flags = 0;
  float age = 0.0f;
  float lifetime = 0.0f;

  float3 position{ 0.0f, 0.0f, 0.0f };
  float3 velocity{ 0.0f, 0.0f, 0.0f };
  float3 acceleration{ 0.0f, 0.0f, 0.0f };
  float3 rotation{ 0.0f, 0.0f, 0.0f };
  float3 angularVelocity{ 0.0f, 0.0f, 0.0f };

  float2 scale{ 1.0f, 1.0f };
  float size = 1.0f;
  float mass = 1.0f;
  float drag = 0.0f;
  float opacity = 1.0f;

  float4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
  float4 custom0{ 0.0f, 0.0f, 0.0f, 0.0f };
  float4 custom1{ 0.0f, 0.0f, 0.0f, 0.0f };
 };


};