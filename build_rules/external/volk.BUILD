cc_library(
    name = "volk",
    srcs = ["volk.c"],
    hdrs = ["volk.h"],
    defines = select({
        "@bazel_tools//src/conditions:windows": ["VK_USE_PLATFORM_WIN32_KHR"],
        "//conditions:default": [],
    }),
    include_prefix = "volk",
    includes = ["."],
    visibility = ["//visibility:public"],
    deps = ["@vulkan_sdk//:vulkan"],
)
