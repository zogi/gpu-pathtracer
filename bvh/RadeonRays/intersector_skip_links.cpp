/**********************************************************************
Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
********************************************************************/
#include "intersector_skip_links.h"

#include "../accelerator/bvh.h"
#include "../accelerator/split_bvh.h"
#include "../primitive/instance.h"
#include "../primitive/mesh.h"
#include "../world/world.h"

#include <algorithm>

// Preferred work group size for Radeon devices
static int const kWorkGroupSize = 64;

namespace RadeonRays {

void BvhBuilder::updateBvh(const World &world)
{
  const int numshapes = (int)world.shapes_.size();
  int numvertices = 0;
  int numfaces = 0;

  // This buffer tracks mesh start index for next stage as mesh face indices are relative to 0
  m_mesh_vertices_start_idx.resize(numshapes);
  m_mesh_faces_start_idx.resize(numshapes);

  // Check options
  auto builder = world.options_.GetOption("bvh.builder");
  auto splits = world.options_.GetOption("bvh.sah.use_splits");
  auto maxdepth = world.options_.GetOption("bvh.sah.max_split_depth");
  auto overlap = world.options_.GetOption("bvh.sah.min_overlap");
  auto tcost = world.options_.GetOption("bvh.sah.traversal_cost");
  auto node_budget = world.options_.GetOption("bvh.sah.extra_node_budget");
  auto nbins = world.options_.GetOption("bvh.sah.num_bins");

  bool use_sah = false;
  bool use_splits = false;
  int max_split_depth = maxdepth ? (int)maxdepth->AsFloat() : 10;
  int num_bins = nbins ? (int)nbins->AsFloat() : 64;
  float min_overlap = overlap ? overlap->AsFloat() : 0.05f;
  float traversal_cost = tcost ? tcost->AsFloat() : 10.f;
  float extra_node_budget = node_budget ? node_budget->AsFloat() : 0.5f;

  if (builder && builder->AsString() == "sah") {
    use_sah = true;
  }

  if (splits && splits->AsFloat() > 0.f) {
    use_splits = true;
  }

  m_bvh.reset(
    use_splits ?
      new SplitBvh(traversal_cost, num_bins, max_split_depth, min_overlap, extra_node_budget) :
      new Bvh(traversal_cost, num_bins, use_sah));

  // Partition the array into meshes and instances
  m_shapes = world.shapes_;
  auto firstinst = std::partition(m_shapes.begin(), m_shapes.end(), [&](Shape const *shape) {
    return !static_cast<ShapeImpl const *>(shape)->is_instance();
  });

  // Count the number of meshes
  const int nummeshes = (int)std::distance(m_shapes.begin(), firstinst);
  // Count the number of instances
  const int numinstances = (int)std::distance(firstinst, m_shapes.end());

  for (int i = 0; i < nummeshes; ++i) {
    Mesh const *mesh = static_cast<Mesh const *>(m_shapes[i]);

    m_mesh_faces_start_idx[i] = numfaces;
    m_mesh_vertices_start_idx[i] = numvertices;

    numfaces += mesh->num_faces();
    numvertices += mesh->num_vertices();
  }

  for (int i = nummeshes; i < nummeshes + numinstances; ++i) {
    Instance const *instance = static_cast<Instance const *>(m_shapes[i]);
    Mesh const *mesh = static_cast<Mesh const *>(instance->GetBaseShape());

    m_mesh_faces_start_idx[i] = numfaces;
    m_mesh_vertices_start_idx[i] = numvertices;

    numfaces += mesh->num_faces();
    numvertices += mesh->num_vertices();
  }

  // We can't avoild allocating it here, since bounds aren't stored anywhere
  std::vector<bbox> bounds(numfaces);

  // We handle meshes first collecting their world space bounds
#pragma omp parallel for
  for (int i = 0; i < nummeshes; ++i) {
    Mesh const *mesh = static_cast<Mesh const *>(m_shapes[i]);

    for (int j = 0; j < mesh->num_faces(); ++j) {
      // Here we directly get world space bounds
      mesh->GetFaceBounds(j, false, bounds[m_mesh_faces_start_idx[i] + j]);
    }
  }

  // Then we handle instances. Need to flatten them into actual geometry.
#pragma omp parallel for
  for (int i = nummeshes; i < nummeshes + numinstances; ++i) {
    Instance const *instance = static_cast<Instance const *>(m_shapes[i]);
    Mesh const *mesh = static_cast<Mesh const *>(instance->GetBaseShape());

    // Instance is using its own transform for base shape geometry
    // so we need to get object space bounds and transform them manually
    matrix m, minv;
    instance->GetTransform(m, minv);

    for (int j = 0; j < mesh->num_faces(); ++j) {
      bbox tmp;
      mesh->GetFaceBounds(j, true, tmp);
      bounds[m_mesh_faces_start_idx[i] + j] = transform_bbox(tmp, m);
    }
  }

  m_bvh->Build(&bounds[0], numfaces);

#ifdef RR_PROFILE
  m_bvh->PrintStatistics(std::cout);
#endif

  m_translator.Process(*m_bvh);

  m_numvertices = numvertices;
  m_numfaces = numfaces;
  m_nummeshes = nummeshes;
  m_numinstances = numinstances;
}


void BvhBuilder::getBuffers(gsl::span<Node> out_nodes, gsl::span<float3> out_vertices, gsl::span<Face> out_faces)
{
  assert(out_nodes.size() == getNodeCount());
  assert(out_vertices.size() == getVertexCount());
  assert(out_faces.size() == getFaceCount());

  const int nummeshes = m_nummeshes;
  const int numinstances = m_numinstances;

  // Copy nodes to output.
  std::copy(m_translator.nodes_.cbegin(), m_translator.nodes_.cend(), out_nodes.begin());

  // Transform and write vertices to output.
  {
    // Here we need to put data in world space rather than object space
    // So we need to get the transform from the mesh and multiply each vertex
    matrix m, minv;

#pragma omp parallel for
    for (int i = 0; i < nummeshes; ++i) {
      // Get the mesh
      Mesh const *mesh = static_cast<Mesh const *>(m_shapes[i]);
      // Get vertex buffer of the current mesh
      float3 const *in_vertices = mesh->GetVertexData();
      // Get mesh transform
      mesh->GetTransform(m, minv);

      //#pragma omp parallel for
      // Iterate thru vertices multiply and append them to GPU buffer
      for (int j = 0; j < mesh->num_vertices(); ++j) {
        out_vertices[m_mesh_vertices_start_idx[i] + j] = transform_point(in_vertices[j], m);
      }
    }

#pragma omp parallel for
    for (int i = nummeshes; i < nummeshes + numinstances; ++i) {
      Instance const *instance = static_cast<Instance const *>(m_shapes[i]);
      // Get the mesh
      Mesh const *mesh = static_cast<Mesh const *>(instance->GetBaseShape());
      // Get vertex buffer of the current mesh
      float3 const *in_vertices = mesh->GetVertexData();
      // Get mesh transform
      instance->GetTransform(m, minv);

      //#pragma omp parallel for
      // Iterate thru vertices multiply and append them to GPU buffer
      for (int j = 0; j < mesh->num_vertices(); ++j) {
        out_vertices[m_mesh_vertices_start_idx[i] + j] = transform_point(in_vertices[j], m);
      }
    }
  }

  // Create face buffer
  {
    // This number is different from the number of faces for some BVHs
    const auto numindices = m_bvh->GetNumIndices();
    // Create face buffer

    // Here the point is to add mesh starting index to actual index contained within the mesh,
    // getting absolute index in the buffer.
    // Besides that we need to permute the faces accorningly to BVH reordering, which
    // is contained within bvh.primids_
    int const *reordering = m_bvh->GetIndices();
    for (size_t i = 0; i < numindices; ++i) {
      int indextolook4 = reordering[i];

      // We need to find a shape corresponding to current face
      auto iter =
        std::upper_bound(m_mesh_faces_start_idx.cbegin(), m_mesh_faces_start_idx.cend(), indextolook4);

      // Find the index of the shape
      int shapeidx = static_cast<int>(std::distance(m_mesh_faces_start_idx.cbegin(), iter) - 1);

      // Get the mesh directly or out of instance
      Mesh const *mesh = nullptr;
      if (shapeidx < nummeshes) {
        mesh = static_cast<Mesh const *>(m_shapes[shapeidx]);
      } else {
        mesh =
          static_cast<Mesh const *>(static_cast<Instance const *>(m_shapes[shapeidx])->GetBaseShape());
      }

      // Get vertex buffer of the current mesh
      Mesh::Face const *myfacedata = mesh->GetFaceData();
      // Find face idx
      int faceidx = indextolook4 - m_mesh_faces_start_idx[shapeidx];
      // Find mesh start idx
      int mystartidx = m_mesh_vertices_start_idx[shapeidx];

      // Copy face data to GPU buffer
      out_faces[i].idx[0] = myfacedata[faceidx].idx[0] + mystartidx;
      out_faces[i].idx[1] = myfacedata[faceidx].idx[1] + mystartidx;
      out_faces[i].idx[2] = myfacedata[faceidx].idx[2] + mystartidx;

      // Optimization: we are putting faceid here
      out_faces[i].shape_id = m_shapes[shapeidx]->GetId();
      out_faces[i].prim_id = faceidx;
    }
  }
}

} // namespace RadeonRays
