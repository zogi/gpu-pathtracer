#version 450

#extension GL_GOOGLE_include_directive: require

layout(binding = 0) readonly buffer Vertices {
  vec4 vertices[];
};

layout(location = 0) out vec4 outputColor;

void main() {
  //outputColor = vec4(0, 0, 1, 1);
  vec4 v = vertices[0];
  outputColor = vec4(v.xyz, 1);
}
