cmake_minimum_required(VERSION 3.20)
project(EcoCampus)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)

include(FetchContent)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        0.9.9.8
)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.2
)
FetchContent_MakeAvailable(nlohmann_json)

set(SOURCES
    src/main.cpp
    src/core/GameLoop.h
    src/core/GameLoop.cpp
    src/core/EventBus.h
    src/core/EventBus.cpp
    src/render/Shader.h
    src/render/Shader.cpp
    src/render/Renderer.h
    src/render/Renderer.cpp
    src/scene/VCRoad.h
    src/scene/VCRoad.cpp
    src/scene/Room.h
    src/scene/Room.cpp
    src/scene/SceneManager.h
    src/scene/SceneManager.cpp
    src/entity/Player.h
    src/entity/Player.cpp
    src/entity/Door.h
    src/entity/Door.cpp
    src/narrative/Narrator.h
    src/narrative/Narrator.cpp
    src/input/InputHandler.h
    src/input/InputHandler.cpp
)

add_executable(EcoCampus WIN32 ${SOURCES})

target_include_directories(EcoCampus PRIVATE src)

target_link_libraries(EcoCampus
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    glm
    nlohmann_json::nlohmann_json
    opengl32
)

# Copy assets to build directory
add_custom_command(TARGET EcoCampus POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:EcoCampus>/assets"
)
