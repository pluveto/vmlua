cmake_minimum_required(VERSION 3.15)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# variables
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_CXX_EXTENSIONS off)


# project
project(
    vmlua
    LANGUAGES CXX
    VERSION 0.0.1
    HOMEPAGE_URL https://github.com/pluveto/vmlua
    DESCRIPTION "Another lua virtual machine"    
)

message(STATUS "> ${PROJECT_NAME} ${PROJECT_VERSION}")
add_executable(${PROJECT_NAME})

aux_source_directory(src PROJ_SOURCES)
target_include_directories(${PROJECT_NAME} PUBLIC include)
target_sources(${PROJECT_NAME} PUBLIC ${PROJ_SOURCES})
target_link_libraries(${PROJECT_NAME} ${CMAKE_CXX_STANDARD_LIBRARIES})

set($ENV{ENV_PROJECT_NAME} ${PROJECT_NAME})