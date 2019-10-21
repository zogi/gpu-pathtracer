cc_library(
    name = "meshoptimizer",
    srcs = [
        "src/allocator.cpp",
        "src/clusterizer.cpp",
        "src/indexcodec.cpp",
        "src/indexgenerator.cpp",
        "src/overdrawanalyzer.cpp",
        "src/overdrawoptimizer.cpp",
        "src/simplifier.cpp",
        "src/spatialorder.cpp",
        "src/stripifier.cpp",
        "src/vcacheanalyzer.cpp",
        "src/vcacheoptimizer.cpp",
        "src/vertexcodec.cpp",
        "src/vfetchanalyzer.cpp",
        "src/vfetchoptimizer.cpp",
    ],
    hdrs = [
        "src/meshoptimizer.h",
    ],
    include_prefix = "meshoptimizer",
    includes = [
        ".",
        "src",
    ],
    strip_include_prefix = "src",
    visibility = ["//visibility:public"],
)
