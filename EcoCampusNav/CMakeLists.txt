cmake_minimum_required(VERSION 3.20)
project(EcoCampusNav VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
    DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

set(SOURCES
    src/main.cpp
    src/core/graph/CampusGraph.cpp
    src/core/graph/Algorithms.cpp
    src/repositories/JsonGraphRepository.cpp
    src/services/NavigationService.cpp
    src/services/ScenarioManager.cpp
    src/services/ComplexityAnalyzer.cpp
    src/services/ResilienceService.cpp
    src/ui/MainWindow.cpp
    src/ui/CampusMapView.cpp
    src/ui/NodeItem.cpp
    src/ui/EdgeItem.cpp
    src/ui/DfsTab.cpp
    src/ui/BfsTab.cpp
    src/ui/ConnectivityTab.cpp
    src/ui/PathfindTab.cpp
    src/ui/ScenarioTab.cpp
    src/ui/ComplexityTab.cpp
    src/ui/ResilienceTab.cpp
)

add_executable(EcoCampusNav ${SOURCES})

target_include_directories(EcoCampusNav PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${nlohmann_json_SOURCE_DIR}
)

target_link_libraries(EcoCampusNav PRIVATE Qt6::Widgets)

configure_file(${CMAKE_SOURCE_DIR}/campus.json ${CMAKE_BINARY_DIR}/campus.json COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/settings.json ${CMAKE_BINARY_DIR}/settings.json COPYONLY)
