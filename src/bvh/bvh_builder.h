#pragma once

#include <gsl/span>

// RadeonRays
#include <math/bbox.h>
#include <math/float2.h>
#include <math/float3.h>
#include <math/int2.h>
#include <math/int3.h>
#include <math/mathutils.h>
#include <math/matrix.h>
#include <math/quaternion.h>
#include <math/ray.h>
#include <primitive/instance.h>
#include <primitive/mesh.h>
#include <world/world.h>

#include <bvh/mesh_view.h>

namespace bvh {

using bbox = RadeonRays::bbox;
using BvhOptions = RadeonRays::Options;

// This implementation uses skip-links BVH, that is:
// Each BVH node is a bbox consisting of the following:
//  - pmin.xyz, pmax.xyz: bounding box of the node
//  - pmin.w: uint32 payload for leafs, 0xFFFFFFFF for non-leafs. The payload can be:
//     - per-mesh BVH: triangle index or range of indices in packed (start, length) form
//     - top-level BVH: instance index or range of indices in packed (start, length) form
//  - pmax.w: uint32 node index of the next neighbor of this node or the nearest ancestor of this
//       node that has a next neighbor, or 0xFFFFFFFF for the root node
// Non-leaf nodes are immediately followed by their first child in the node array.

// Build an skip-links BVH with the supplied leaf bounding boxes.
std::vector<bbox> build_bvh(gsl::span<bbox> leaf_bounds, const BvhOptions &options);

} // namespace bvh
