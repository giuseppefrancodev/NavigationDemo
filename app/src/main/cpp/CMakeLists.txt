cmake_minimum_required(VERSION 3.22.1)
project(navigation_engine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Download pugixml if not present
include(FetchContent)
FetchContent_Declare(
        pugixml
        GIT_REPOSITORY https://github.com/zeux/pugixml.git
        GIT_TAG v1.12
)
FetchContent_MakeAvailable(pugixml)

# Add include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Define source files directly
add_library(navigation_engine SHARED
        navigation_engine.cpp
        route_matcher.cpp
        location_filter.cpp
        road_graph.cpp
        routing_engine.cpp
        osm_parser.cpp
)

# Find android log library
find_library(log-lib log)
find_library(android-lib android)

# Link against required libraries
target_link_libraries(navigation_engine
        ${log-lib}
        ${android-lib}
        pugixml
)