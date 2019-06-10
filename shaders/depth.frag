#version 450

#extension GL_GOOGLE_include_directive: require

#include "RadeonRays/bvh.glslh"

struct DebugVars {
  int debugVarInt1;
};

layout(push_constant) uniform block
{
	DebugVars debugVars;
};

layout(location = 0) in vec2 texCoords;
layout(location = 0) out vec4 outputColor;

void main() {
  //outputColor = vec4(0, 0, 1, 1);
  //bbox node = Nodes[0];
  //outputColor = vec4(node.pmax.xyz, 1);
  //vec4 v = Vertices[1];
  //outputColor = vec4(v.xyz, 1);
  outputColor = vec4(texCoords, 0, 1);

  // Construct ray in view space.
  ray r;
  r.extra.x = ~0; // shape mask
  r.extra.y = 1;  // ray is active
  float maxDist = 1000.0;
  r.o = vec4(0, 0, 0, maxDist);
  // TODO: compute ray.xy properly using fov and aspect ratio
  float time = 0; // for motion blur
  vec2 dxy = texCoords - 0.5;
  dxy.y *= -1;
  //dxy *= 2;
  //vec2 dxy = vec2(0, 0);
  r.d = vec4(dxy, 1, time);

  // TODO: transform ray into world space.
  r.o.z -= 5;

#if 0
  // Intersect with scene.
  Intersection isect;
  IntersectSceneClosest(r, isect);
  if (isect.shapeid == -1) {
    outputColor = vec4(0, 0, 0, 1);
  } else {
    outputColor = vec4(1, 1, 1, 1);
  }
#endif

#if 1
  // Intersect with leaf node.
  BvhNode node = Nodes[debugVars.debugVarInt1];
  Intersection isect;
  isect.shapeid = -1;
  isect.uvwt.w = maxDist;
  IntersectLeafClosest(node, r, isect);
  if (isect.shapeid == -1) {
    outputColor = vec4(0, 0, 0, 1);
  } else {
    outputColor = vec4(1, 1, 1, 1);
  }
#endif

#if 0
  // Visualize BVH node.
  BvhNode node = Nodes[0];
  Intersection isect;
  isect.uvwt.w = maxDist;
  const vec3 invdir  = vec3(1.f, 1.f, 1.f)/r.d.xyz;
  if (IntersectBox(r, invdir, node, isect.uvwt.w)) {
    outputColor = vec4(1, 1, 1, 1);
  } else {
    outputColor = vec4(0, 0, 0, 1);
  }
#endif

#if 0
  // Visualize triangle.
  vec3 v1 = Vertices[0].xyz;
  vec3 v2 = Vertices[1].xyz;
  vec3 v3 = Vertices[2].xyz;
  Intersection isect;
  isect.uvwt.w = maxDist;
  if (IntersectTriangle(r, v1, v2, v3, isect)) {
    outputColor = vec4(1, 1, 1, 1);
  } else {
    outputColor = vec4(0, 0, 0, 1);
  }
#endif
}
