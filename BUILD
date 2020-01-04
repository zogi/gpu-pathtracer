load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "shaders_config",
    hdrs = [
        "shaders/config-inc.h",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "granite",
    includes = [
        "third_party",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//third_party/granite",
        "//third_party/granite:granite_glfw",
    ],
)
