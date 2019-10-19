cc_library(
    name = "volk",
    srcs = ["volk.c"],
    hdrs = ["volk.h"],
    includes = ['.'],
    include_prefix = 'volk',
    defines = select({
        "@bazel_tools//src/conditions:windows": ["VK_USE_PLATFORM_WIN32_KHR"],
        "//conditions:default": [],
    }),
    visibility = ["//visibility:public"],
    deps = ["@vulkan_sdk//:vulkan"],
)
