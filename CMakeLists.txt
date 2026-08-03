cmake_minimum_required(VERSION 3.18)
project(CPPLUASERVER VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#find_package(sol2 CONFIG REQUIRED)
find_package(Lua CONFIG REQUIRED)
find_path(SOL2_INCLUDE_DIRS "sol/abort.hpp")

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp")
add_executable(${PROJECT_NAME} ${SOURCES})

#target_link_libraries(SOL_TESTING PRIVATE sol2::sol2)
target_include_directories(${PROJECT_NAME} PRIVATE ${LUA_INCLUDE_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${SOL2_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${LUA_LIBRARIES})

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/src/config.ini"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/endpoints.yaml"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMENT "Copying config.ini and endpoints.yaml to executable directory"
)

set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>")