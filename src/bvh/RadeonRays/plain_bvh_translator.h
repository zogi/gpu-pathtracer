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
#ifndef PLAIN_BVH_TRANSLATOR_H
#define PLAIN_BVH_TRANSLATOR_H

#include <map>

#include "accelerator/bvh.h"
#include "radeon_rays.h"

#include "math/float3.h"
#include "math/matrix.h"
#include "math/quaternion.h"

namespace RadeonRays {
/// This class translates pointer based BVH representation into
/// index based one suitable for feeding to GPU or any other accelerator
//
class PlainBvhTranslator {
 public:
  // Constructor
  PlainBvhTranslator() = default;

  struct PrimitiveRange {
    int first;
    int count;
  };

  // Plain BVH node
  struct Node {
    // Node's bounding box
    bbox bounds;
    // If leafnode: the indices of primitives contained in this node
    // If internal node: {-1, -1}
    PrimitiveRange primitives;
  };

  void Flush();
  void Process(Bvh &bvh);
  void Process(Bvh const **bvhs, int const *offsets, int numbvhs);
  void UpdateTopLevel(Bvh const &bvh);

  const std::vector<Node> &getNodes() const { return nodes_; }

 private:
  std::vector<Node> nodes_;
  // std::vector<int> extra_;
  std::vector<int> roots_;
  int nodecnt_ = 0;
  int root_ = 0;

 private:
  int ProcessNode(Bvh::Node const *node);
  // int ProcessNode(Bvh::Node const *n, int offset);

  PlainBvhTranslator(PlainBvhTranslator const &) = delete;
  PlainBvhTranslator &operator=(PlainBvhTranslator const &) = delete;
};
} // namespace RadeonRays


#endif // PLAIN_BVH_TRANSLATOR_H
