#version 450

#extension GL_GOOGLE_include_directive: require

// Triangle strip.
vec2 verts[] = { vec2(-1, -1), vec2(-1, 1), vec2(1, -1), vec2(1, 1) };

layout(location = 0) out vec2 texCoords;

void main()
{
  // TODO: remove this: multiply by 0.8 to see the quad for now.
  vec2 v = verts[gl_VertexIndex];
  gl_Position = vec4(v * 0.8, 1, 1);
  texCoords = 0.5 * v + 0.5;
}
