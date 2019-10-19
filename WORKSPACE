load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")
load("//build_rules/vulkan_sdk:repository_rule.bzl", "vulkan_sdk")

vulkan_sdk(
    name = "vulkan_sdk",
)

new_git_repository(
    name = "vulkan_headers",
    build_file = "//build_rules/external:vulkan_headers.BUILD",
    remote = "https://github.com/KhronosGroup/Vulkan-Headers",
    tag = "v1.1.125",
)

new_git_repository(
    name = "granite",
    build_file = "//build_rules/external:granite.BUILD",
    commit = "a67049c459e19adfb79920d3402e0ed52451ef6a",
    remote = "https://github.com/zogi/Granite",
)

new_git_repository(
    name = "spirv_cross",
    build_file = "//build_rules/external:spirv_cross.BUILD",
    remote = "https://github.com/KhronosGroup/SPIRV-Cross",
    tag = "2019-09-06",
)

new_git_repository(
    name = "stb",
    build_file = "//build_rules/external:stb.BUILD",
    commit = "052dce117ed989848a950308bd99eef55525dfb1",
    remote = "https://github.com/nothings/stb",
)

new_git_repository(
    name = "tinyobjloader",
    build_file = "//build_rules/external:tinyobjloader.BUILD",
    remote = "https://github.com/syoyo/tinyobjloader",
    tag = "v1.0.7",
)

new_git_repository(
    name = "volk",
    build_file = "//build_rules/external:volk.BUILD",
    commit = "453c4de373c57f752d0c778b314a000fce09170e",
    remote = "https://github.com/zeux/volk",
)
