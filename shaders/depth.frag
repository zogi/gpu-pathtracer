#version 450

#extension GL_GOOGLE_include_directive: require

#include "config-inc.h"
#include "RadeonRays/bvh.glslh"

struct ViewData {
  mat4 view_transform;
  vec3 eye_pos;
  // Extent of the image at unit distance from camera center.
  vec2 image_size_norm;
};

layout(std140, binding = 4, row_major) uniform ViewDataBlock
{
	ViewData view_data;
};

struct DebugVars {
  float debug_var_cam_offset;
  float debug_var_scale;
  int debug_var_int_1;
  float debug_var_float_1;
  float debug_var_float_2;
};

layout(push_constant) uniform DebugVarsBlock
{
	DebugVars debug_vars;
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

  r.o.xyz = vec3(0, 0, 0);
  r.o.w = 1000.0; // max distance

  vec2 dxy = (texCoords - 0.5);
  dxy *= view_data.image_size_norm;
  dxy.y = -dxy.y;
  dxy *= debug_vars.debug_var_scale;
  r.d.xyz = vec3(dxy.xy, -1);
  r.d.w = 0; // time for motion blur

  // Transform to world space.
  r.o.xyz = view_data.eye_pos;
  // TODO: use quaternion?
  r.d.xyz = mat3(view_data.view_transform) * r.d.xyz;
  r.d.xyz = normalize(r.d.xyz);

  r.o.x += debug_vars.debug_var_cam_offset;

#if 1
  // Intersect with scene.
  Intersection isect;
  IntersectSceneClosest(r, isect);
  if (isect.shapeid == -1) {
    outputColor = vec4(0, 0, 0, 1);
  } else {
    float x = smoothstep(isect.uvwt.w, debug_vars.debug_var_float_1, debug_vars.debug_var_float_2);
    outputColor = vec4(x, x, x, 1);
  }
#endif

#if 0
  // Visualize nodes.

  BvhNode node = Nodes[debug_vars.debug_var_int_1];
  Intersection isect;
  isect.shapeid = -1;
  isect.uvwt.w = maxDist;
  bool intersection = false;
  if (LEAFNODE(node)) {
    IntersectLeafClosest(node, r, isect);
    intersection = (isect.shapeid != -1);
  } else {
    const vec3 invdir  = vec3(1.f, 1.f, 1.f)/r.d.xyz;
    intersection = IntersectBox(r, invdir, node, isect.uvwt.w);
  }

  if (intersection) {
    outputColor = vec4(1, 1, 1, 1);
  } else {
    outputColor = vec4(0, 0, 0, 1);
  }
#endif

#if 0
  // Visualize faces.

  Face face = Faces[debug_vars.debug_var_int_1];
  vec3 v1 = Vertices[face.idx0].xyz;
  vec3 v2 = Vertices[face.idx1].xyz;
  vec3 v3 = Vertices[face.idx2].xyz;
  Intersection isect;
  isect.uvwt.w = maxDist;
  if (IntersectTriangle(r, v1, v2, v3, isect)) {
    outputColor = vec4(1, 1, 1, 1);
  } else {
    outputColor = vec4(0, 0, 0, 1);
  }
#endif
}
