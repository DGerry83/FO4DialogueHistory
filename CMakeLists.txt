cmake_minimum_required(VERSION 4.3.0)

project(FO4DialogueHistory VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# CommonLibF4 (LucaDotGit "All Versions" fork) — CMake-native; consumed via
# FetchContent pinned to e337150. fmt/spdlog resolve through the vcpkg
# toolchain file (triplet x64-windows-static-md-release — see
# docs/Build-and-Compile.md). A commit-hash pin needs GIT_SHALLOW left off.
# Runtime coverage: OG 1.10.163 / NG 1.10.984 / AE 1.11.191 (VR off).
include(FetchContent)

set(COMMONLIB_RUNTIME_VR OFF CACHE BOOL "" FORCE)
# COMMONLIB_RUNTIME_OG/NG/AE stay at the upstream defaults (ON).

FetchContent_Declare(commonlibf4
	GIT_REPOSITORY https://github.com/LucaDotGit/CommonLibF4.git
	GIT_TAG e337150ac993daaa9d089fcddf7a6ce9a6bad42f
	GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(commonlibf4)

if(MSVC OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
	add_compile_options(/EHsc)
endif()

# Target: shared library FO4DialogueHistory -> deployed as
# Data/F4SE/Plugins/FO4DialogueHistory.dll

add_library(${PROJECT_NAME} SHARED
	src/Plugin.cpp
	src/Composition.cpp
	src/Core/Dialogue/PayloadBuilder.cpp
	src/Core/Dialogue/CosaveCodec.cpp
	src/Core/Dialogue/StressTestDataGenerator.cpp
	src/Core/Geometry/PanelGeometry.cpp
	src/Core/Input/ScanCodeMap.cpp
	src/Application/Capture/CapturePipeline.cpp
	src/Application/View/ViewController.cpp
	src/Application/Input/HotkeyController.cpp
	src/Application/StressTest/StressTestInjector.cpp
	src/Infrastructure/Hooks/SubtitleHook.cpp
	src/Infrastructure/Hooks/QuestResolver.cpp
	src/Infrastructure/Cosave/CosaveStore.cpp
	src/Infrastructure/Prisma/PrismaViewBridge.cpp
	src/Infrastructure/Settings/IniSettingsStore.cpp
	src/Infrastructure/Logging/Log.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE src)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)
target_link_libraries(${PROJECT_NAME} PRIVATE CommonLibF4::CommonLibF4)
target_compile_definitions(${PROJECT_NAME} PRIVATE
	F4DH_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
	F4DH_VERSION_MINOR=${PROJECT_VERSION_MINOR}
	F4DH_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# Tests fetch Catch2 via FetchContent; guard keeps plugin builds free of
# test dependencies (e.g. -DF4DH_BUILD_TESTS=OFF).
option(F4DH_BUILD_TESTS "Build the Core unit tests (Catch2)" ON)
if(F4DH_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests/Core.Tests)
endif()
