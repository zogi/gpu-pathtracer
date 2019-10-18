def _impl(ctx):
    if ctx.os.name.startswith("windows"):
        sdk_root = ctx.os.environ["VULKAN_SDK"]
        if not sdk_root:
            fail("VULKAN_SDK envvar is not set")

        lib = "Lib/vulkan-1.lib"
        root = "vulkan_sdk"
        ctx.symlink(sdk_root, root)
        glslangValidator = root + "/bin/glslangValidator.exe"

    elif ctx.os.name.startswith("linux"):
        ctx.download_and_extract(
          url = "https://sdk.lunarg.com/sdk/download/1.1.82.1/linux/vulkansdk-linux-x86_64-1.1.82.1.tar.gz?u=",
          output = "vulkan_sdk",
          type = "tar.gz",
          sha256 = "9f6ff7e86aef4e4d6d95d8fab23f7734e0c02c2febd0113dc29b8e78cd48743b",
        )
        lib = "lib/libvulkan.so.1"
        root = "vulkan_sdk/1.1.82.1/x86_64"
        glslangValidator = root + "/Bin/glslangValidator"

    else:
        fail("OS is not supported: '{}'".format(ctx.os.name))

    ctx.symlink(glslangValidator, "glslangValidator")

    ctx.file("WORKSPACE", content="")
    content_fmt = """
cc_library(
    name = "vulkan",
    srcs = ["{root}/{lib}"],
    hdrs = glob(["{root}/include/**"]),
    visibility = ["//visibility:public"],
    strip_include_prefix = "{root}/include",
)
exports_files(["glslangValidator"])
"""
    content = content_fmt.format(root=root, lib=lib)
    ctx.file("BUILD.bazel", content=content)

vulkan_sdk = repository_rule(
    local = False,
    implementation = _impl,
)
