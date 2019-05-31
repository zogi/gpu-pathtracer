#version 450

#extension GL_GOOGLE_include_directive: require

// Triangle strip.
vec2 verts[] = { vec2(-1, -1), vec2(-1, 1), vec2(1, -1), vec2(1, 1) };

void main()
{
  // TODO: remove this: multiply by 0.8 to see the quad for now.
  gl_Position = vec4(verts[gl_VertexIndex] * 0.8, 1, 1);
}
