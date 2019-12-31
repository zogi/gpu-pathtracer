#pragma once

#include <gsl/span>

// RadeonRays
#include <math/bbox.h>
#include <math/float3.h>
#include <math/mathutils.h>
#include <math/matrix.h>
#include <primitive/mesh.h>

namespace bvh {

using float3 = RadeonRays::float3;

template <typename Index>
struct TriangleFaceT {
  union {
    Index idx[3];
    struct {
      Index i0, i1, i2;
    };
  };
};
using TriangleFace32 = TriangleFaceT<uint32_t>;

struct TriangleMeshView {
  gsl::span<const float3> vertices;
  gsl::span<const TriangleFace32> faces;
};

#if 0
bbox calc_transformed_face_bounds(const MeshView &mesh_view, const Mesh::Face &face, const matrix &transform) {
  static_assert(Mesh::FaceType::LINE == 1);
  static_assert(Mesh::FaceType::TRIANGLE == 2);
  static_assert(Mesh::FaceType::QUAD == 3);

  const auto transformed_point = [&mesh_view, &face, &transform](int i) {
    const auto vidx = face.idx[i];
    return transform_point(mesh_view.vertices[vidx], transform);
  };

  bbox res = bbox(transformed_point(0));
  for (int i = 1; i <= (int)face.type_; ++i) {
    res.grow(transformed_point(i));
  }
  return res;
}
#endif

} // namespace bvh
