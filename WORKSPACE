load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository", "new_git_repository")
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
    name = "meshoptimizer",
    build_file = "//build_rules/external:meshoptimizer.BUILD",
    commit = "bdc3006532dd29b03d83dc819e5fa7683815b88e",
    remote = "https://github.com/zeux/meshoptimizer",
)

new_git_repository(
    name = "rapidjson",
    build_file = "//build_rules/external:rapidjson.BUILD",
    commit = "6a6bed2759d42891f9e29a37b21315d3192890ed",
    remote = "https://github.com/Tencent/rapidjson",
)

new_git_repository(
    name = "shaderc",
    build_file = "//build_rules/external:shaderc.BUILD",
    commit = "621605ce2644d55ab74cb27a8afef97dc40f1b0f",
    remote = "https://github.com/google/shaderc",
)

new_git_repository(
    name = "spirv_cross",
    build_file = "//build_rules/external:spirv_cross.BUILD",
    remote = "https://github.com/KhronosGroup/SPIRV-Cross",
    tag = "2019-09-06",
)

local_repository(
    name = "spirv_headers",
    path = "third-party/spirv_headers",
)

local_repository(
    name = "spirv_tools",
    path = "third-party/spirv_tools",
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
