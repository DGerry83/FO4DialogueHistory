cmake_minimum_required(VERSION 3.31)

project(FO4DialogueHistory VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CommonLibF4 (libxse fork) is xmake-only upstream, so FetchContent
# materializes the sources and the static targets are defined here, mirroring
# upstream xmake.lua (src/**.cpp, include/, per-target PCH force-include).
# Pinned commits are recorded in .flybywire PROGRESS_LOG.
include(FetchContent)

set(SPDLOG_USE_STD_FORMAT ON CACHE BOOL "" FORCE)
set(SPDLOG_WCHAR_SUPPORT ON CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_Declare(spdlog
	GIT_REPOSITORY https://github.com/gabime/spdlog.git
	GIT_TAG 486b55554f11c9cccc913e11a87085b2a91f706f # v1.16.0
	GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(commonlibf4
	GIT_REPOSITORY https://github.com/libxse/commonlibf4.git
	GIT_TAG 7c8c6f8a349ac85abf6fc8a7e5a71732d9339954
	GIT_SUBMODULES "lib/commonlib-shared"
)
FetchContent_MakeAvailable(commonlibf4)

set(COMMONLIBF4_SHARED_DIR "${commonlibf4_SOURCE_DIR}/lib/commonlib-shared")

file(GLOB_RECURSE COMMONLIB_SHARED_SOURCES CONFIGURE_DEPENDS
	"${COMMONLIBF4_SHARED_DIR}/src/*.cpp"
)
add_library(commonlib-shared STATIC ${COMMONLIB_SHARED_SOURCES})
target_include_directories(commonlib-shared PUBLIC "${COMMONLIBF4_SHARED_DIR}/include")
target_link_libraries(commonlib-shared PUBLIC spdlog::spdlog)
target_precompile_headers(commonlib-shared PRIVATE "${COMMONLIBF4_SHARED_DIR}/src/REX/PCH.h")
target_compile_features(commonlib-shared PUBLIC cxx_std_23)
# REX::W32 import forwarders resolve against these SDK import libraries
# (mirrors upstream commonlib-shared xmake.lua add_syslinks).
target_link_libraries(commonlib-shared PUBLIC
	advapi32
	bcrypt
	d3d11
	d3dcompiler
	dbghelp
	dxgi
	ole32
	shell32
	user32
	version
	ws2_32
)

file(GLOB_RECURSE COMMONLIBF4_SOURCES CONFIGURE_DEPENDS
	"${commonlibf4_SOURCE_DIR}/src/*.cpp"
)
add_library(commonlibf4 STATIC ${COMMONLIBF4_SOURCES})
target_include_directories(commonlibf4 PUBLIC "${commonlibf4_SOURCE_DIR}/include")
target_link_libraries(commonlibf4 PUBLIC commonlib-shared)
target_precompile_headers(commonlibf4 PRIVATE "${commonlibf4_SOURCE_DIR}/include/F4SE/Impl/PCH.h")
target_compile_features(commonlibf4 PUBLIC cxx_std_23)
# clang 22 promotes -Wreturn-mismatch to an error; upstream RegistrationMap.cpp
# relies on the MSVC warning. Keep it a warning on this dependency only.
target_compile_options(commonlibf4 PRIVATE -Wno-error=return-mismatch)

if(MSVC OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
	add_compile_options(/EHsc)
endif()

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
target_link_libraries(${PROJECT_NAME} PRIVATE commonlibf4)
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
