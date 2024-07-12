// swift-tools-version:5.10
import PackageDescription

let package = Package(
  name: "swift-mosquitto",
  platforms: [
    .macOS(.v14),
    .visionOS(.v1),
    .iOS(.v17),
    .tvOS(.v17),
    .watchOS(.v10)
  ],
  products: [
    .library(name: "SwiftMosquitto", targets: ["SwiftMosquitto"])
  ],
  targets: [
    .target(name: "mosquitto_config"),
    .target(
      name: "mosquittopp",
      dependencies: [
        .target(name: "mosquitto_config")
      ],
      path: ".",
      exclude: [
        "Sources/mosquitto/apps",
        "Sources/mosquitto/client",
        "Sources/mosquitto/cmake",
        "Sources/mosquitto/doc",
        "Sources/mosquitto/docker",
        "Sources/mosquitto/examples",
        "Sources/mosquitto/installer",
        "Sources/mosquitto/logo",
        "Sources/mosquitto/man",
        "Sources/mosquitto/misc",
        "Sources/mosquitto/plugins",
        "Sources/mosquitto/security",
        "Sources/mosquitto/service",
        "Sources/mosquitto/snap",
        "Sources/mosquitto/test",
        "Sources/mosquitto/www",
        "Sources/mosquitto/src",
        "Sources/mosquitto/lib/handle_connack.c",
        "Sources/mosquitto/lib/handle_auth.c",
        "Sources/mosquitto/lib/handle_publish.c",
        "Sources/mosquitto/lib/handle_disconnect.c",
        "Sources/mosquitto/lib/logging_mosq.c",
        "Sources/mosquitto/lib/read_handle.c",
        "Sources/mosquitto/lib/CMakeLists.txt",
        "Sources/mosquitto/lib/Makefile",
        "Sources/mosquitto/lib/cpp/CMakeLists.txt",
        "Sources/mosquitto/lib/cpp/Makefile",
        "Sources/mosquitto/lib/linker.version",
      ],
      sources: [
        "Sources/mosquitto/lib",
      ],
      publicHeadersPath: "Sources/mosquitto/lib/cpp",
      cSettings: [
        .headerSearchPath("Sources/mosquitto/lib"),
        .headerSearchPath("Sources/mosquitto/include"),
        .headerSearchPath("Sources/mosquitto/deps"),
        .headerSearchPath("Sources/mosquitto/src")
      ]
    ),
    .target(
      name: "mosquitto_shim",
      dependencies: [
        .target(name: "mosquittopp")
      ]
    ),
    .target(
      name: "mosquitto",
      dependencies: [
        .target(name: "mosquitto_shim")
      ],
      exclude: [
        "apps",
        "client",
        "cmake",
        "doc",
        "docker",
        "examples",
        "installer",
        "logo",
        "man",
        "misc",
        "plugins",
        "security",
        "service",
        "snap",
        "test",
        "www",
        "lib",
        "src/plugin_defer.c",
        "src/mosquitto.c",
        "src/CMakeLists.txt",
        "src/Makefile",
        "src/linker-macosx.syms",
        "src/linker.syms",
      ],
      sources: [
        "src"
      ],
      cSettings: [
        .headerSearchPath("deps"),
        .headerSearchPath("src"),
        .headerSearchPath("lib"),
        .define("WITH_BROKER", to: "1"),
        .define("VERSION", to: "\"2.0.18\""),
      ]
    ),
    .target(
      name: "mosquitto_cxx",
      dependencies: [
        .target(name: "mosquitto")
      ],
      cSettings: [
        .define("WITH_BROKER", to: "1")
      ]
    ),
    .target(
      name: "SwiftMosquitto",
      dependencies: [
        .target(name: "mosquitto_cxx")
      ],
      swiftSettings: swiftSettings
    ),
    .testTarget(
      name: "SwiftMosquittoTests",
      dependencies: [
        .target(name: "SwiftMosquitto")
      ],
      swiftSettings: swiftSettings
    )
  ],
  cxxLanguageStandard: .cxx17
)

var swiftSettings: [SwiftSetting] { [
  .enableUpcomingFeature("DisableOutwardActorInference"),
  .enableExperimentalFeature("StrictConcurrency"),
  .interoperabilityMode(.Cxx)
] }
