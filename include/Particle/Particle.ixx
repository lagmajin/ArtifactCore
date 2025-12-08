module;
export module Particle;

export namespace ArtifactCore
{
 struct float3 {
  float x, y, z;
 };
	
 struct Particle {
  float3 pos;
  float3 vel;
  float  life;
  float  size;
  //float4 color;
 };


};