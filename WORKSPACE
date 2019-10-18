load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")
load("//build_rules/vulkan_sdk:repository_rule.bzl", "vulkan_sdk")

vulkan_sdk(
    name = "vulkan_sdk",
)

new_git_repository(
    name = "stb",
    remote = "https://github.com/nothings/stb",
    commit = "052dce117ed989848a950308bd99eef55525dfb1",
    build_file = "//build_rules/external:BUILD.stb",
)

new_git_repository(
    name = "tinyobjloader",
    remote = "https://github.com/syoyo/tinyobjloader",
    tag = "v1.0.7",
    build_file = "//build_rules/external:BUILD.tinyobjloader",
)

new_git_repository(
    name = "volk",
    remote = "https://github.com/zeux/volk",
    commit = "453c4de373c57f752d0c778b314a000fce09170e",
    build_file = "//build_rules/external:BUILD.volk",
)
