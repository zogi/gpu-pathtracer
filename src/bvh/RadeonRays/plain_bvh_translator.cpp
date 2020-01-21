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
#include "plain_bvh_translator.h"

#include "except/except.h"
#include "primitive/instance.h"
#include "primitive/mesh.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <stack>

namespace {
inline float floatBitsFromInt(uint32_t i) { return *(float *)&i; }
inline uint32_t intFromFloatBits(float x) { return *(uint32_t *)&x; }
} // unnamed namespace

namespace RadeonRays {
void PlainBvhTranslator::Process(Bvh &bvh) {
  // WARNING: this is crucial in order for the nodes not to migrate in memory as push_back adds nodes
  nodecnt_ = 0;
  int newsize = bvh.m_nodecnt;
  nodes_.resize(newsize);
  // extra_.resize(newsize);

  // Check if we have been initialized
  assert(bvh.m_root);

  // Save current root position
  int rootidx = 0;

  // Process root
  ProcessNode(bvh.m_root);

  assert(nodecnt_ == nodes_.size());

  // Set next ptr
  // TODO: use 0xffffffff
  nodes_[rootidx].bounds.pmax.w = -1;

  //
  for (int i = rootidx; i < (int)nodes_.size(); ++i) {
    // TODO: use 0xffffffff
    if (nodes_[i].bounds.pmin.w != -1.f) {
      nodes_[i + 1].bounds.pmax.w = nodes_[i].bounds.pmin.w;
      // TODO: use intFromFloatBits
      nodes_[(int)(nodes_[i].bounds.pmin.w)].bounds.pmax.w = nodes_[i].bounds.pmax.w;
    }
  }

  const auto &reordering = bvh.GetIndices();
  for (int i = rootidx; i < (int)nodes_.size(); ++i) {
    auto &node = nodes_[i];
    // TODO: use 0xffffffff
    if (node.bounds.pmin.w == -1.f) {
      // nodes_[i].bounds.pmin.w = (float)extra_[i];
      // TODO: use floatBitsFromInt

      // Here it is assumed that primitive indices
      // [reordering[s], reordering[s+1], ..., reordering[s+n-1]]
      // are contiguous.
      // s = node.primitives.start, n = node.primitives.count
      const auto startidx = node.primitives.first;
      for (int j = 1; j < node.primitives.count; ++j) {
        assert(reordering[startidx + j] == reordering[startidx] + j);
      }

      const uint32_t primitive_idx = reordering[startidx];
      node.bounds.pmin.w = float(primitive_idx);
      // nodes_[i].bounds.pmin.w = floatBitsFromInt(extra_[i]);
    } else {
      // TODO: use 0xffffffff
      node.bounds.pmin.w = -1.f;
    }
  }
}

void PlainBvhTranslator::UpdateTopLevel(Bvh const &bvh) {
  assert(false);
  // TODO: use common logic with Process.
  return;

#if 0
  nodecnt_ = root_;

  // Process root
  ProcessNode(bvh.m_root);

  // Set next ptr
  nodes_[root_].bounds.pmax.w = -1;

  for (int j = root_; j < root_ + bvh.m_nodecnt; ++j) {
    // TODO: use 0xffffffff
    if (nodes_[j].bounds.pmin.w != -1.f) {
      nodes_[j + 1].bounds.pmax.w = nodes_[j].bounds.pmin.w;
      // TODO: use intFromFloatBits
      nodes_[(int)(nodes_[j].bounds.pmin.w)].bounds.pmax.w = nodes_[j].bounds.pmax.w;
    }
  }

  for (int j = root_; j < root_ + bvh.m_nodecnt; ++j) {
    // TODO: use 0xffffffff
    if (nodes_[j].bounds.pmin.w == -1.f) {
      // TODO: use floatBitsFromInt
      nodes_[j].bounds.pmin.w = (float)extra_[j];
    } else {
      // TODO: use 0xffffffff
      nodes_[j].bounds.pmin.w = -1.f;
    }
  }
#endif
}

void PlainBvhTranslator::Process(Bvh const **bvhs, int const *offsets, int numbvhs) {
  assert(false);
  // TODO: use common logic with Process.
  return;

#if 0
  // First of all count the number of required nodes for all BVH's
  int nodecnt = 0;
  for (int i = 0; i < numbvhs + 1; ++i) {
    if (!bvhs[i]) {
      continue;
    }

    nodecnt += bvhs[i]->m_nodecnt;
  }

  nodes_.resize(nodecnt);
  extra_.resize(nodecnt);
  roots_.resize(numbvhs);

  for (int i = 0; i < numbvhs; ++i) {
    if (!bvhs[i]) {
      continue;
    }

    int currentroot = nodecnt_;

    roots_[i] = currentroot;

    // Process root
    ProcessNode(bvhs[i]->m_root, offsets[i]);

    // Set next ptr
    // TODO: use 0xffffffff
    nodes_[currentroot].bounds.pmax.w = -1;

    for (int j = currentroot; j < currentroot + bvhs[i]->m_nodecnt; ++j) {
      // TODO: use 0xffffffff
      if (nodes_[j].bounds.pmin.w != -1.f) {
        nodes_[j + 1].bounds.pmax.w = nodes_[j].bounds.pmin.w;
        // TODO: use intFromFloatBits
        nodes_[(int)(nodes_[j].bounds.pmin.w)].bounds.pmax.w = nodes_[j].bounds.pmax.w;
      }
    }

    for (int j = currentroot; j < currentroot + bvhs[i]->m_nodecnt; ++j) {
      // TODO: use 0xffffffff
      if (nodes_[j].bounds.pmin.w == -1.f) {
        // TODO: use floatBitsFromInt
        nodes_[j].bounds.pmin.w = (float)extra_[j];
      } else {
        // TODO: use 0xffffffff
        nodes_[j].bounds.pmin.w = -1.f;
      }
    }
  }

  // The final one
  root_ = nodecnt_;

  // Process root
  ProcessNode(bvhs[numbvhs]->m_root);

  // Set next ptr
  // TODO: use 0xffffffff
  nodes_[root_].bounds.pmax.w = -1;

  for (int j = root_; j < root_ + bvhs[numbvhs]->m_nodecnt; ++j) {
    // TODO: use 0xffffffff
    if (nodes_[j].bounds.pmin.w != -1.f) {
      nodes_[j + 1].bounds.pmax.w = nodes_[j].bounds.pmin.w;
      // TODO: use intFromFloatBits
      nodes_[(int)(nodes_[j].bounds.pmin.w)].bounds.pmax.w = nodes_[j].bounds.pmax.w;
    }
  }

  for (int j = root_; j < root_ + bvhs[numbvhs]->m_nodecnt; ++j) {
    // TODO: use 0xffffffff
    if (nodes_[j].bounds.pmin.w == -1.f) {
      // TODO: use floatBitsFromInt
      nodes_[j].bounds.pmin.w = (float)extra_[j];
    } else {
      // TODO: use 0xffffffff
      nodes_[j].bounds.pmin.w = -1.f;
    }
  }
#endif
}

int PlainBvhTranslator::ProcessNode(Bvh::Node const *n) {
  int idx = nodecnt_;
  // std::cout << "Index " << idx << "\n";
  Node &node = nodes_[nodecnt_];
  node.bounds = n->bounds;
  // int &extra = extra_[nodecnt_];
  nodecnt_++;

  if (n->type == Bvh::kLeaf) {
    int startidx = n->startidx;
    node.primitives.first = startidx;
    node.primitives.count = n->numprims;
    // TODO: use 0xffffffff
    node.bounds.pmin.w = -1.f;
  } else {
    ProcessNode(n->lc);
    // TODO: use floatBitsFromInt
    node.bounds.pmin.w = (float)ProcessNode(n->rc);
    node.primitives.first = -1;
    node.primitives.count = -1;
  }

  return idx;
}

#if 0
int PlainBvhTranslator::ProcessNode(Bvh::Node const *n, int offset) {
  int idx = nodecnt_;
  Node &node = nodes_[nodecnt_];
  node.bounds = n->bounds;
  // int &extra = extra_[nodecnt_];
  nodecnt_++;

  if (n->type == Bvh::kLeaf) {
    int startidx = n->startidx + offset;
    // extra = (startidx << 4) | (n->numprims & 0xF);
    node.primitives.first = startidx;
    node.primitives.count = n->numprims;
    // TODO: use 0xffffffff
    node.bounds.pmin.w = -1.f;
  } else {
    ProcessNode(n->lc, offset);
    // TODO: use floatBitsFromInt
    node.bounds.pmin.w = (float)ProcessNode(n->rc, offset);
  }

  return idx;
}
#endif

void PlainBvhTranslator::Flush() {
  nodecnt_ = 0;
  root_ = 0;
  roots_.resize(0);
  nodes_.resize(0);
  // extra_.resize(0);
}
} // namespace RadeonRays
