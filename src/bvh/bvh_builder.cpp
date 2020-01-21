#include <bvh/bvh_builder.h>

#include <memory>

// RadeonRays
#include <accelerator/split_bvh.h>

#include "RadeonRays/plain_bvh_translator.h"

namespace bvh {

using Bvh = RadeonRays::Bvh;
using SplitBvh = RadeonRays::SplitBvh;

// Factory method for RadeonRays BVH implementations.
std::unique_ptr<Bvh> make_bvh(const BvhOptions &options) {
  // Check options
  auto builder = options.GetOption("bvh.builder");
  auto splits = options.GetOption("bvh.sah.use_splits");
  auto maxdepth = options.GetOption("bvh.sah.max_split_depth");
  auto overlap = options.GetOption("bvh.sah.min_overlap");
  auto tcost = options.GetOption("bvh.sah.traversal_cost");
  auto node_budget = options.GetOption("bvh.sah.extra_node_budget");
  auto nbins = options.GetOption("bvh.sah.num_bins");

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

  if (use_splits) {
    return std::make_unique<SplitBvh>(
      traversal_cost, num_bins, max_split_depth, min_overlap, extra_node_budget);
  } else {
    return std::make_unique<Bvh>(traversal_cost, num_bins, use_sah);
  }
}

std::vector<bbox> build_bvh(gsl::span<bbox> leaf_bounds, const BvhOptions &options) {
  // Scale up bounds a bit to avoid cracks when tracing rays against the scene.
  // TODO: move this into bvh->Build to avoid this additional copy.
  std::vector<bbox> scaled_leafs(leaf_bounds.begin(), leaf_bounds.end());
  for (auto &leaf : scaled_leafs) {
    constexpr float kBoundsScale = 1 + 1e-4f;
    const auto center = leaf.center();
    leaf.pmin = center + (leaf.pmin - center) * kBoundsScale;
    leaf.pmax = center + (leaf.pmax - center) * kBoundsScale;
  }
  // Build BVH in tree-like representation.
  auto bvh = make_bvh(options);
  bvh->Build(scaled_leafs.data(), scaled_leafs.size());
  // bvh->PrintStatistics(std::cout);

  // Translate to linear skip-links representation.
  RadeonRays::PlainBvhTranslator translator;
  translator.Process(*bvh);

  // Copy out the linearized nodes.
  std::vector<bbox> res;
  res.reserve(translator.getNodes().size());
  for (const auto &translator_node : translator.getNodes()) {
    res.push_back(translator_node.bounds);
  }
  return res;
}

} // namespace bvh
