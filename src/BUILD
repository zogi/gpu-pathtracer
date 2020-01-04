load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_binary(
    name = "main",
    srcs = [
        "main_granite.cpp",
    ],
    deps = [
        ":bvh",
        # ":camera",
        "//:granite",
        "//:shaders_config",
        "@tinyobjloader",
        "@volk",
    ],
)

cc_library(
    name = "bvh",
    srcs = [
        "bvh/RadeonRays/intersector_skip_links.cpp",
        "bvh/RadeonRays/plain_bvh_translator.cpp",
        "bvh/bvh_builder.cpp",
    ],
    hdrs = [
        "bvh/RadeonRays/intersector_skip_links.h",
        "bvh/RadeonRays/plain_bvh_translator.h",
        "bvh/bvh_builder.h",
        "bvh/mesh_view.h",
    ],
    includes = [
        ".",
    ],
    deps = [
        "@radeon_rays//:radeon_rays_base",
    ],
)

cc_library(
    name = "camera",
    srcs = [
        "camera.cpp",
    ],
    hdrs = [
        "camera.h",
    ],
    deps = [
        "//:granite",
    ],
)
