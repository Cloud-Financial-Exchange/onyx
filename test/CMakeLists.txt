cmake_minimum_required(VERSION 3.4 FATAL_ERROR)

project(jasper-test)
set (PARENT_DIR ${PROJECT_SOURCE_DIR}/..)

find_package(Git REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Protobuf REQUIRED)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(gtest-build)
include(libs)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_COMPILER ${GCC_PATH})
set(CMAKE_CXX_OUTPUT_EXTENSION_REPLACE ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -Wall ${LIBRARY_CFLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -Wall ")

# file(GLOB SOURCES ${PARENT_DIR}/src/*.cpp)
# list(REMOVE_ITEM SOURCES ${PARENT_DIR}/src/main.c)

file(GLOB TEST_SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)
set (SOURCES 
    ${PARENT_DIR}/src/build/message.pb.cc
    ${PARENT_DIR}/src/utils/ZMQSocket.cpp
    ${PARENT_DIR}/src/utils/common.hpp
    ${PARENT_DIR}/src/utils/tree.hpp)

set(PROJECT_SOURCES 
    ${TEST_SOURCES} 
    ${SOURCES})

set(PROJECT_INCLUDES
    ${PARENT_DIR}/src
    ${PARENT_DIR}/src/utils/
    ${PARENT_DIR}/src/build/
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/mock/include
    ${LIBRARY_INCLUDES}
)

include_directories(${PROJECT_INCLUDES})
add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
target_compile_definitions(${PROJECT_NAME} PRIVATE ZMQ_BUILD_DRAFT_API=1)
link_directories(${LIBRARY_DIRS})
target_link_libraries(${PROJECT_NAME} gtest gmock_main ${LIBRARIES})

set_property(DIRECTORY PROPERTY ADDITIONAL_CLEAN_FILES
    "${PROJECT_SOURCE_DIR}/build/CMakeFiles"
    "${PROJECT_SOURCE_DIR}/build/CMakeCache.txt"
    "${PROJECT_SOURCE_DIR}/build/Makefile"
    "${PROJECT_SOURCE_DIR}/build/cmake_install.cmake"
    # "${PROJECT_SOURCE_DIR}/build/compile_commands.json"
)

add_custom_target("run"
    DEPENDS ${PROJECT_NAME}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMAND ./bin/${PROJECT_NAME}
)

add_custom_target("debug"
    DEPENDS ${PROJECT_NAME}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMAND gdb -tui -x ./GDBCOMMANDS --args ./bin/${PROJECT_NAME}
)

if(DOCS)
    find_package(Doxygen REQUIRED)
    add_custom_target(
        docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${PROJECT_SOURCE_DIR}/docs/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/docs
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
endif()
