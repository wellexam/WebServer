cmake_minimum_required(VERSION 3.22)
project(WebServer)

set(CMAKE_CXX_STANDARD 14)
include_directories(./*)
file(GLOB SOURCES "*.cpp")
MESSAGE(STATUS "sources =  ${SOURCES}")

add_executable(WebServer ${SOURCES})

IF (CMAKE_BUILD_TYPE STREQUAL Debug)
    ADD_DEFINITIONS(-DDEBUG)
ENDIF()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
find_package(MYSQL)


MESSAGE(STATUS "CMAKE_MODULE_PATH = ${CMAKE_MODULE_PATH}.")

target_link_libraries(${PROJECT_NAME} PRIVATE ${MYSQL_LIBRARY} Threads::Threads)
