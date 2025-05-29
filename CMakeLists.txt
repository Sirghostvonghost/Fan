cmake_minimum_required(VERSION 3.10)
project(AirscapeFanController)

set(CMAKE_CXX_STANDARD 17)

# Find required packages
find_package(CURL REQUIRED)
find_package(nlohmann_json 3.2.0 QUIET)

# If nlohmann_json wasn't found, we'll use FetchContent to get it
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(json
        URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
    )
    FetchContent_MakeAvailable(json)
endif()

# Add executable
add_executable(fan_controller fan_controller.cpp)

# Link libraries
target_link_libraries(fan_controller PRIVATE CURL::libcurl)
if(nlohmann_json_FOUND)
    target_link_libraries(fan_controller PRIVATE nlohmann_json::nlohmann_json)
else()
    target_link_libraries(fan_controller PRIVATE nlohmann_json::nlohmann_json)
endif()