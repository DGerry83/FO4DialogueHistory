cmake_minimum_required(VERSION 3.31)

project(FO4DialogueHistory VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Milestone 1: fetch dependencies and finish wiring.
# - CommonLibF4 (libxse fork): https://github.com/libxse/commonlibf4 — via
#   FetchContent or add_subdirectory(extern/commonlibf4); provides RE::,
#   F4SE::, REL::ID / Address Library support.
# - clib-util (hook helpers: stl::hook_function_prologue / write_thunk_call).
# - Catch2 v3 for tests/Core.Tests (milestone 2).
#
# Target: shared library FO4DialogueHistory -> deployed as
# Data/F4SE/Plugins/FO4DialogueHistory.dll

add_library(${PROJECT_NAME} SHARED
	src/Plugin.cpp
	src/Composition.cpp
	src/Core/Dialogue/PayloadBuilder.cpp
	src/Application/Capture/CapturePipeline.cpp
	src/Application/View/ViewController.cpp
	src/Application/Input/HotkeyController.cpp
	src/Infrastructure/Hooks/SubtitleHook.cpp
	src/Infrastructure/Prisma/PrismaViewBridge.cpp
	src/Infrastructure/Settings/IniSettingsStore.cpp
	src/Infrastructure/Logging/Log.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE src)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)

enable_testing()
# add_subdirectory(tests/Core.Tests)  # milestone 2
