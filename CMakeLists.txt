cmake_minimum_required(VERSION 3.20)

# ============================================================================
# Portable vcpkg Toolchain Detection (Fixed & Simplified)
# ============================================================================
set(VCPKG_CANDIDATES
        "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        "$ENV{USERPROFILE}/.vcpkg-clion/vcpkg/scripts/buildsystems/vcpkg.cmake"
        "$ENV{HOME}/.vcpkg-clion/vcpkg/scripts/buildsystems/vcpkg.cmake"
)

foreach(_candidate ${VCPKG_CANDIDATES})
  if(EXISTS "${_candidate}")
    set(CMAKE_TOOLCHAIN_FILE "${_candidate}" CACHE STRING "" FORCE)
    message(STATUS "✓ Using vcpkg toolchain: ${_candidate}")
    break()
  endif()
endforeach()

if(CMAKE_TOOLCHAIN_FILE)
  set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "" FORCE)
endif()

# ============================================================================
project(EcoCampusNav VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# Dependencies
# ============================================================================
find_package(raylib CONFIG REQUIRED)

if(NOT TARGET raylib::raylib)
  add_library(raylib::raylib ALIAS raylib)
endif()
message(STATUS "✓ Raylib found")

include(FetchContent)

FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
        imgui
        URL https://github.com/ocornut/imgui/archive/refs/tags/v1.90.4.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_DIR ${imgui_SOURCE_DIR})
set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC ${IMGUI_DIR})

# ============================================================================
# rlImGui (Local)
# ============================================================================
set(RLIMGUI_DIR "${CMAKE_SOURCE_DIR}/external/rlImGui")
set(RLIMGUI_CPP "${RLIMGUI_DIR}/rlImGui.cpp")
set(RLIMGUI_H   "${RLIMGUI_DIR}/rlImGui.h")

if(EXISTS ${RLIMGUI_CPP} AND EXISTS ${RLIMGUI_H})
  message(STATUS "✓ Found rlImGui in ${RLIMGUI_DIR}")
  add_library(rlImGui STATIC ${RLIMGUI_CPP})
  target_include_directories(rlImGui PUBLIC ${RLIMGUI_DIR})
  target_link_libraries(rlImGui PUBLIC imgui raylib::raylib)
else()
  message(FATAL_ERROR "rlImGui not found in ${RLIMGUI_DIR}")
endif()

# ============================================================================
# Project Sources
# ============================================================================
set(SOURCES
        src/main.cpp
        src/core/application/ApplicationSession.cpp
        src/core/application/WindowInitializer.cpp
        src/core/application/AudioInitializer.cpp
        src/core/graph/CampusGraph.cpp
        src/core/graph/Algorithms.cpp
        src/repositories/JsonGraphRepository.cpp
        src/services/NavigationService.cpp
        src/services/DataManager.cpp
        src/services/AudioManager.cpp
        src/services/ScenarioManager.cpp
        src/services/ComplexityAnalyzer.cpp
        src/services/MusicService.cpp
        src/services/ResilienceService.cpp
        src/services/SoundEffectService.cpp
        src/services/EasterEggManager.cpp
        src/services/TransitionService.cpp
        src/services/TmjLoader.cpp
        src/services/StringUtils.cpp
        src/services/InterestZoneLoader.cpp
        src/services/AssetPathResolver.cpp
        src/services/ScenePlanService.cpp
        src/services/WalkablePathService.cpp
        src/services/SpriteAnimationService.cpp
        src/services/RuntimeTextService.cpp
        src/services/MapRenderService.cpp
        src/services/DestinationCatalog.cpp
        src/services/RuntimeBlockerService.cpp
        src/services/IntroTourConfigLoader.cpp
        src/services/initialization/FloorLinkLoader.cpp
        src/services/initialization/IntroTourSetup.cpp
        src/services/initialization/SceneBootstrap.cpp
        src/runtime/InputManager.cpp
        src/runtime/GameController.cpp
        src/runtime/StartMenuController.cpp
        src/runtime/GameplayLoopController.cpp
        src/runtime/SceneManager.cpp
        src/runtime/UIManager.cpp
        src/runtime/RuntimeInputManager.cpp
        src/runtime/RuntimeNavigationManager.cpp
        src/ui/TabManager.cpp
)

add_executable(EcoCampusNav ${SOURCES})

target_include_directories(EcoCampusNav PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${IMGUI_DIR}
)

target_link_libraries(EcoCampusNav PRIVATE
        raylib::raylib
        nlohmann_json::nlohmann_json
        imgui
        rlImGui
)

# ============================================================================
# Runtime Data & Assets
# ============================================================================
configure_file(${CMAKE_SOURCE_DIR}/campus.json ${CMAKE_BINARY_DIR}/campus.json COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/settings.json ${CMAKE_BINARY_DIR}/settings.json COPYONLY)

add_custom_command(TARGET EcoCampusNav POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:EcoCampusNav>/assets
        COMMENT "Copying assets directory to runtime folder"
)


message(STATUS "======================================")
message(STATUS "EcoCampusNav (Raylib+ImGui) configured")
message(STATUS "======================================")
