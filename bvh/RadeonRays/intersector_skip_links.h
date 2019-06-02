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
/**
    \file intersector_skip_links.h
    \author Dmitry Kozlov
    \version 1.0
    \brief Intersector implementation based on BVH with skip links.

    IntersectorSkipLinks implementation is based on the following paper:
    "Efficiency Issues for Ray Tracing" Brian Smits
    http://www.cse.chalmers.se/edu/year/2016/course/course/TDA361/EfficiencyIssuesForRayTracing.pdf

    Intersector is using binary BVH with a single bounding box per node. BVH layout guarantees
    that left child of an internal node lies right next to it in memory. Each BVH node has a
    skip link to the node traversed next. The traversal pseude code is

        while(addr is valid)
        {
            node <- fetch next node at addr
            if (rays intersects with node bbox)
            {
                if (node is leaf)
                    intersect leaf
                else
                {
                    addr <- addr + 1 (follow left child)
                    continue
                }
            }

            addr <- skiplink at node (follow next)
        }

    Pros:
        -Simple and efficient kernel with low VGPR pressure.
        -Can traverse trees of arbitrary depth.
    Cons:
        -Travesal order is fixed, so poor algorithmic characteristics.
        -Does not benefit from BVH quality optimizations.

    ===
    Modified to contain only the CPU BVH builder function.
 */

#pragma once

#include <memory>

#include <gsl/span>

#include "../translator/plain_bvh_translator.h"
#include "math/float3.h"

namespace RadeonRays {
class Bvh;
class World;

using Node = PlainBvhTranslator::Node;

// Position
using Vertex = float3;

struct Face {
  // Up to 3 indices
  int idx[3];
  // Shape ID
  int shape_id;
  // Primitive ID
  int prim_id;
};

class BvhBuilder {
 public:
  /// Build BVH from geometry in World.
  /// @post get*Count and get*BufferSizeBytes will return an updated value after this call.
  /// @param world World contaning geometry from which to build/update the bvh.
  void updateBvh(const World &world);

  int getNodeCount() const { return m_translator.nodecnt_; }
  size_t getNodeBufferSizeBytes() const { return getNodeCount() * sizeof(Node); }
  int getVertexCount() const { return m_numvertices; }
  size_t getVertexBufferSizeBytes() const { return getVertexCount() * sizeof(Vertex); }
  int getFaceCount() const { return m_numfaces; }
  size_t getFaceBufferSizeBytes() const { return getFaceCount() * sizeof(Face); }

  /// Write BVH buffer data to specified buffers.
  /// @pre \ref updateBvh has been called successfully at least once.
  /// @pre out_nodes.size() == getNodeCount()
  /// @pre out_vertices.size() == getVertexCount()
  /// @pre out_faces.size() == getFaceCount()
  void getBuffers(gsl::span<Node> out_nodes, gsl::span<Vertex> out_vertices, gsl::span<Face> out_faces);

 private:
  std::unique_ptr<Bvh> m_bvh;
  PlainBvhTranslator m_translator;
  std::vector<Shape const *> m_shapes;
  std::vector<int> m_mesh_vertices_start_idx;
  std::vector<int> m_mesh_faces_start_idx;
  int m_numvertices = 0;
  int m_numfaces = 0;
  int m_nummeshes = 0;
  int m_numinstances = 0;
};

} // namespace RadeonRays
