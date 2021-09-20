// swift-tools-version:5.3
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "slottie",
    products: [
        // Products define the executables and libraries a package produces, and make them visible to other packages.
        .library(
            name: "slottie",
            targets: ["slottie"]),
    ],
    dependencies: [
        // Dependencies declare other packages that this package depends on.
        // .package(url: /* package url */, from: "1.0.0"),
    ],
    targets: [
        // Targets are the basic building blocks of a package. A target can define a module or a test suite.
        // Targets can depend on other targets in this package, and on products in packages this package depends on.
        .target(
            name: "slottie",
            dependencies: ["rlottie"],
            path: "Swift"
        ),
        .target(
            name: "rlottie",
            path: "src",
            exclude: [
                "vector/pixman/pixman-arm-neon-asm.S",
                "wasm/",
                "vector/pixman",
                "vector/vdrawhelper_neon.cpp",
                "lottie/rapidjson/msinttypes",
                "CMakeLists.txt",
                "binding/CMakeLists.txt",
                "binding/c/CMakeLists.txt",
                "binding/c/meson.build",
                "binding/meson.build",
                "inc/CMakeLists.txt",
                "inc/meson.build",
                "lottie/CMakeLists.txt",
                "lottie/meson.build",
                "meson.build",
                "vector/CMakeLists.txt",
                "vector/freetype/CMakeLists.txt",
                "vector/freetype/meson.build",
                "vector/meson.build",
                "vector/stb/CMakeLists.txt",
                "vector/stb/meson.build",
            ],
            publicHeadersPath: "inc_c",
            cxxSettings: [
                .headerSearchPath("inc"),
                .headerSearchPath("vector"),
                .headerSearchPath("vector/freetype"),
            ]
        )
    ],
    cxxLanguageStandard: .cxx14
)
