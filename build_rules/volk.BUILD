cc_library(
    name = "volk",
    srcs = ["volk.c"],
    hdrs = ["volk.h"],
    includes = ["."],
    local_defines = select({
        "@bazel_tools//src/conditions:windows": [
            "VK_USE_PLATFORM_WIN32_KHR",
        ],
        "//conditions:default": [
            # TODO
        ],
    }),
    visibility = ["//visibility:public"],
    deps = ["@vulkan_headers"],
)
