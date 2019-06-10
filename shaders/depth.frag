#version 450

#extension GL_GOOGLE_include_directive: require

struct bbox {
    vec4 pmin;
    vec4 pmax;
};

layout(binding = 0) readonly buffer Nodes {
  bbox nodes[];
};

layout(binding = 1) readonly buffer Vertices {
  vec4 vertices[];
};

struct Face {
    // Vertex indices
    int idx0;
    int idx1;
    int idx2;
    int shapeidx;
    // Primitive ID
    int id;
    // Idx count
    int cnt;

    ivec2 padding;
};

layout(binding = 2) readonly buffer Faces {
  Face faces[];
};

layout(location = 0) out vec4 outputColor;

void main() {
  //outputColor = vec4(0, 0, 1, 1);
  //bbox node = nodes[0];
  //outputColor = vec4(node.pmax.xyz, 1);
  vec4 v = vertices[1];
  outputColor = vec4(v.xyz, 1);
}
