cc_binary(
    name = "main",
    srcs = [
        "main_granite.cpp"
    ],
    deps = [
        ":bvh",
        # ":camera",
        "//:granite",
        "@tinyobjloader",
        "@volk",
    ],
)

cc_library(
    name = "bvh",
    srcs = [
        "bvh/RadeonRays/intersector_skip_links.cpp",
        "bvh/RadeonRays/plain_bvh_translator.cpp",
    ],
    hdrs = [
        "bvh/RadeonRays/intersector_skip_links.h",
        "bvh/RadeonRays/plain_bvh_translator.h",
        "bvh/bvh_builder.h",
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
