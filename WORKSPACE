load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")
load("//build_rules/vulkan_sdk:repository_rule.bzl", "vulkan_sdk")

vulkan_sdk(
    name = "vulkan_sdk",
)

new_git_repository(
    name = "tinyobjloader",
    remote = "https://github.com/syoyo/tinyobjloader",
    tag = "v1.0.7",
    build_file = "//build_rules/external:BUILD.tinyobjloader",
)
