cmake_minimum_required(VERSION 3.22)
project(WebServer)

set(CMAKE_CXX_STANDARD 14)
include_directories(./*)

add_executable(WebServer main.cpp)

IF (CMAKE_BUILD_TYPE STREQUAL Debug)
    ADD_DEFINITIONS(-DDEBUG)
ENDIF()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
find_package(MYSQL)


MESSAGE(STATUS "CMAKE_MODULE_PATH = ${CMAKE_MODULE_PATH}.")

target_link_libraries(${PROJECT_NAME} PRIVATE ${MYSQL_LIBRARY} Threads::Threads)
