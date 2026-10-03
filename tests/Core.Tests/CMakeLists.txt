include(FetchContent)

# Catch2 v3 — pinned v3.9.1. Only fetched when tests are enabled.
set(CATCH_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CATCH_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CATCH_BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(catch2
	GIT_REPOSITORY https://github.com/catchorg/Catch2.git
	GIT_TAG 644821ce28cb25d7992a4d0375b1d83214392592 # v3.9.1
	GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(catch2)

# Engine-free: links only Core sources (PayloadBuilder, CosaveCodec) plus
# Catch2. Never link commonlibf4/spdlog here — Core must stay testable in isolation.
add_executable(Core.Tests
	DialogueBufferTests.cpp
	ConversationFilterTests.cpp
	PayloadBuilderTests.cpp
	CosaveCodecTests.cpp
	PanelGeometryTests.cpp
	ViewFilterStateTests.cpp
	AliasTokensTests.cpp
	StressTestDataGeneratorTests.cpp
	ScanCodeMapTests.cpp
	${CMAKE_SOURCE_DIR}/src/Core/Dialogue/PayloadBuilder.cpp
	${CMAKE_SOURCE_DIR}/src/Core/Dialogue/CosaveCodec.cpp
	${CMAKE_SOURCE_DIR}/src/Core/Dialogue/StressTestDataGenerator.cpp
	${CMAKE_SOURCE_DIR}/src/Core/Geometry/PanelGeometry.cpp
	${CMAKE_SOURCE_DIR}/src/Core/Settings/ViewFilterState.cpp
	${CMAKE_SOURCE_DIR}/src/Core/Input/ScanCodeMap.cpp
)

target_include_directories(Core.Tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Core.Tests PRIVATE Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(Core.Tests)
