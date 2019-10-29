cc_library(
    name = "volk",
    srcs = ["volk.c"],
    hdrs = ["volk.h"],
    defines = select({
        "@bazel_tools//src/conditions:windows": [
            "VK_USE_PLATFORM_WIN32_KHR",
        ],
        "//conditions:default": [
            # TODO
        ],
    }),
    includes = ["."],
    visibility = ["//visibility:public"],
    deps = ["@vulkan_headers"],
)
