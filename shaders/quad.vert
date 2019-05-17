#version 450

#extension GL_GOOGLE_include_directive: require

// Triangle strip.
vec2 verts[] = { vec2(-1, -1), vec2(-1, 1), vec2(1, -1), vec2(1, 1) };

void main()
{
  gl_Position = vec4(verts[gl_VertexIndex], 1, 1);
}
