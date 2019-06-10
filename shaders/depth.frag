#version 450

#extension GL_GOOGLE_include_directive: require

#include "RadeonRays/bvh.glslh"

layout(location = 0) out vec4 outputColor;

void main() {
  //outputColor = vec4(0, 0, 1, 1);
  //bbox node = Nodes[0];
  //outputColor = vec4(node.pmax.xyz, 1);
  vec4 v = Vertices[1];
  outputColor = vec4(v.xyz, 1);
}
