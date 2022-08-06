cmake_minimum_required(VERSION 3.22)
project(WebServer)

set(CMAKE_CXX_STANDARD 14)
include_directories(./*)
include_directories(./*/*)
file(GLOB SOURCES "*.cpp")
file(GLOB SOURCE "*/*.cpp")

add_executable(WebServer ${SOURCES} ${SOURCE})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
